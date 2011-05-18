Window parent;

while(1)
 {
   // There is a part here that says to get the next event and store it in "event"

   switch(event.type)
    {
      case 'CreateNotify':
       parent = XCreateSimpleWindow(display, RootWindow(display, screen_num), event.xcreatewindow.x, event.xcreatewindow.y, event.xcreatewindow.width + 30, event.xcreatewindow.height + 30, 1, BlackPixel(display, screen_num), WhitePixel(display, screen_num));
       XReparentWindow(display, event.xcreatewindow.window, parent, 15, 15);
       XMoveWindow(display, parent, 100, 100);
       break;
      default:
       break;
    }
 }