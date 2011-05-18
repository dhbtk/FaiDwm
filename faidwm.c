/* FaiDWM, based on TinyWM and Fluxbox
released under the WTFPL: this code is yours as much as it is mine
 */

#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

int anotherWMRunning(Display* dpy, XErrorEvent* event)
{
	fprintf(stderr, "An error occured while querying the X server: \
	other window manager already running on display %s", DisplayString(dpy));
	
	exit(1);
}

int main()
{
	Display * dpy;
	Window root;
	XWindowAttributes attr;

	/* we use this to save the pointer's state at the beginning of the
	move/resize.
	*/
	XButtonEvent grabbed_pointer_start;


	/* return failure status if we can't connect */
	if(!(dpy = XOpenDisplay(0x0))) return 1;

	/* you'll usually be referencing the root window a lot.  this is a somewhat
	naive approach that will only work on the default screen.  most people
	only have one screen, but not everyone.  if you run multi-head without
	xinerama then you quite possibly have multiple screens. (i'm not sure
	about vendor-specific implementations, like nvidia's)
	
	many, probably most window managers only handle one screen, so in
	reality this isn't really *that* naive.
	
	if you wanted to get the root window of a specific screen you'd use
	RootWindow(), but the user can also control which screen is our default:
	if they set $DISPLAY to ":0.foo", then our default screen number is
	whatever they specify "foo" as.
	*/
	root = DefaultRootWindow(dpy);

	/* you could also include keysym.h and use the XK_F1 constant instead of
	the call to XStringToKeysym, but this method is more "dynamic."  imagine
	you have config files which specify key bindings.  instead of parsing
	the key names and having a huge table or whatever to map strings to XK_*
	constants, you can just take the user-specified string and hand it off
	to XStringToKeysym.  XStringToKeysym will give you back the appropriate
	keysym or tell you if it's an invalid key name.
	
	a keysym is basically a platform-independent numeric representation of a
	key, like "F1", "a", "b", "L", "5", "Shift", etc.  a keycode is a
	numeric representation of a key on the keyboard sent by the keyboard
	driver (or something along those lines -- i'm no hardware/driver expert)
	to X.  so we never want to hard-code keycodes, because they can and will
	differ between systems.
	*/
	/*
	XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("F1")), Mod1Mask, root,
			True, GrabModeAsync, GrabModeAsync);
	*/

	/* XGrabKey and XGrabButton are basically ways of saying "when this
	combination of modifiers and key/button is pressed, send me the events."
	so we can safely assume that we'll receive Alt+F1 events, Alt+Button1
	events, and Alt+Button3 events, but no others.  You can either do
	individual grabs like these for key/mouse combinations, or you can use
	XSelectInput with KeyPressMask/ButtonPressMask/etc to catch all events
	of those types and filter them as you receive them.
	*/
//	/*
	XGrabButton(dpy, 1, Mod1Mask, root, True, ButtonPressMask, GrabModeAsync,
			GrabModeAsync, None, None);
	XGrabButton(dpy, 3, Mod1Mask, root, True, ButtonPressMask, GrabModeAsync,
			GrabModeAsync, None, None);
//	*/
	
	/*catch errors selecting some mutually-exclusive that might be held by an existing wm*/
	XErrorHandler old = XSetErrorHandler((XErrorHandler) anotherWMRunning);
	
	/*set which events we want to hear about*/
	XSelectInput(dpy, root, ColormapChangeMask | EnterWindowMask | PropertyChangeMask |
		SubstructureRedirectMask | KeyPressMask | KeyReleaseMask |
		ButtonPressMask | ButtonReleaseMask | SubstructureNotifyMask);
	
	/*restore the old error handler*/
	XSetErrorHandler(old);
	
	XEvent ev;
	for(;;)
	{
		/* this is the most basic way of looping through X events; you can be
		more flexible by using XPending(), or ConnectionNumber() along with
		select() (or poll() or whatever floats your boat).
		*/
		XNextEvent(dpy, &ev);
		
		/*
		i was a little confused about .window vs. .subwindow for a while,
		but a little RTFMing took care of that.  our passive grabs above
		grabbed on the root window, so since we're only interested in events
		for its child windows, we look at .subwindow.  when subwindow ==
		None, that means that the window the event happened in was the same
		window that was grabbed on -- in this case, the root window.
		*/
		printf("\nEvent: type=%d \n", ev.type);
		switch(ev.type)
		{
			case KeyPress:
				printf("KeyPress\n");
				if(ev.xkey.subwindow != None)
				{
					XRaiseWindow(dpy, ev.xkey.subwindow);
				}
				break;
			case ButtonPress:
				printf("ButtonPress\n");
				if(ev.xbutton.subwindow != None)
				{
					printf("grab\n");
					/* now we take command of the pointer, looking for motion and
					button release events.
					*/
					XGrabPointer(dpy, ev.xbutton.subwindow, True,
							PointerMotionMask|ButtonReleaseMask, GrabModeAsync,
							GrabModeAsync, None, None, CurrentTime);

					/* we "remember" the position of the pointer at the beginning of
					our move/resize, and the size/position of the window.  that way,
					when the pointer moves, we can compare it to our initial data
					and move/resize accordingly.
					*/
					XGetWindowAttributes(dpy, ev.xbutton.subwindow, &attr);
					grabbed_pointer_start = ev.xbutton;
				}
				break;
			case MotionNotify:
				printf("MotionNotify\n");
				/* the only way we'd receive a motion notify event is if we already did
				a pointer grab and we're in move/resize mode, so we assume that. */
				int xdiff, ydiff;

				/* here we "compress" motion notify events.  if there are 10 of
				them waiting, it makes no sense to look at any of them but the
				most recent.  in some cases -- if the window is really big or
				things are just acting slowly in general -- failing to do this
				can result in a lot of "drag lag."
			
				for window managers with things like desktop switching, it can
				also be useful to compress EnterNotify events, so that you don't
				get "focus flicker" as windows shuffle around underneath the
				pointer.
				*/
				while(XCheckTypedEvent(dpy, MotionNotify, &ev));

				/* now we use the stuff we saved at the beginning of the
				move/resize and compare it to the pointer's current position to
				determine what the window's new size or position should be.
			
				if the initial button press was button 1, then we're moving.
				otherwise it was 3 and we're resizing.
			
				we also make sure not to go negative with the window's
				dimensions, resulting in "wrapping" which will make our window
				something ridiculous like 65000 pixels wide (often accompanied
				by lots of swapping and slowdown).
			
				even worse is if we get "lucky" and hit a width or height of
				exactly zero, triggering an X error.  so we specify a minimum
				width/height of 1 pixel.
				*/
				xdiff = ev.xbutton.x_root - grabbed_pointer_start.x_root;
				ydiff = ev.xbutton.y_root - grabbed_pointer_start.y_root;
				XMoveResizeWindow(dpy, ev.xmotion.window,
					attr.x + (grabbed_pointer_start.button==1 ? xdiff : 0),
					attr.y + (grabbed_pointer_start.button==1 ? ydiff : 0),
					MAX(1, attr.width + (grabbed_pointer_start.button==3 ? xdiff : 0)),
					MAX(1, attr.height + (grabbed_pointer_start.button==3 ? ydiff : 0)));
				
				break;
			case ButtonRelease:
				printf("ButtonRelease\n");
				/* like motion notifies, the only way we'll receive a button release is
				during a move/resize, due to our pointer grab.  this ends the
				move/resize.
				*/
				XUngrabPointer(dpy, CurrentTime);
				break;
			case ConfigureRequest:
				printf("ConfigureRequest\n");
				//from fluxbox: if the window is queued to be destroyed, ignore/deny the configure request
				XEvent destroyev;
				if (XCheckTypedWindowEvent(dpy, ev.xconfigurerequest.window, DestroyNotify, &destroyev))
				{
					XPutBackEvent(dpy, &destroyev);
					break;
				}
				
				XWindowChanges xwc;
				
				xwc.x = ev.xconfigurerequest.x;
				xwc.y = ev.xconfigurerequest.y;
				xwc.width = ev.xconfigurerequest.width;
				xwc.height = ev.xconfigurerequest.height;
				xwc.border_width = ev.xconfigurerequest.border_width;
				xwc.sibling = ev.xconfigurerequest.above;
				xwc.stack_mode = ev.xconfigurerequest.detail;
				
				XConfigureWindow(dpy,
				ev.xconfigurerequest.window,
				ev.xconfigurerequest.value_mask, &xwc);
				break;
			case ConfigureNotify:
				printf("ConfigureNotify\n");
				break;
			case MapRequest:
				printf("MapRequest\n");
				//setWmState(NormalState);
				XMapWindow(dpy, ev.xmaprequest.window);
				break;
			case MapNotify:
				printf("MapNotify\n");
				if(ev.xmap.override_redirect)
				{
					break;
				}
				
				/*
				otherwise do something?
				or just take note and pass the event on to the handler
				*/
				
				break;
			case PropertyNotify:
				printf("PropertyNotify\n");
				//from fluxbox:
				/*
				WinClient *client = findClient(event.xproperty.window);
				if (client)
				propertyNotifyEvent(*client, event.xproperty.atom);
				*/
				break;
			case ColormapNotify:
				printf("ColormapNotify\n");
				XInstallColormap(ev.xcolormap.display, ev.xcolormap.colormap);
				break;
		}
	}
    XSelectInput(dpy, root, NoEventMask);
}

