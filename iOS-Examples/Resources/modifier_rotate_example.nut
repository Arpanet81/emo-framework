local stage = emo.Stage();
local event = emo.Event();

/*
 * This example shows loading sprites with modifier.
 */
class Loading {

    circle = emo.Sprite("loading.png");
	// 16x16 text sprite with 2 pixel border and 1 pixel margin
	text = emo.TextSprite("font_16x16.png",
		" !\"c*%#'{}@+,=./0123456789:;[|]?&ABCDEFGHIJKLMNOPQRSTUVWXYZ",
		16, 16, 2, 1);
    
	/*
	 * Called when this class is loaded
	 */
    function onLoad() {
        print("Loading::onLoad"); 
		
		// Below statements is an example of multiple screen density support.
		// (i.e. Retina vs non-Retina, cellular phone vs tablet device).
		if (stage.getWindowWidth() >= 640) {
			// if the screen has large display, scale contents twice
			// that makes the stage size by half.
			// This examples shows how to display similar-scale images
			// on Retina and non-Retina display.
			stage.setContentScale(2);
		}
		
		text.setText("  LOADING..");
		
		// move sprite to the center of the screen
		circle.moveCenter(stage.getWindowWidth() / 2, stage.getWindowHeight() / 2);
		text.moveCenter(stage.getWindowWidth()  / 2,
				(stage.getWindowHeight() / 2) + circle.getHeight() + text.getHeight());
		
		// set z-order (move text to front of the circle)
		circle.setZ(0);
		text.setZ(1);

		// load sprite to the screen
		text.load();
        circle.load();
        
        // rotate the block from 0 to 360 degree in 1 seconds using Linear equation with infinite loop.
        circle.addModifier(emo.RotateModifier(0, 360, 1000, emo.easing.Linear, -1));
		
		// change alpha color of the text in 2 seconds
		// using CubicIn and CubicOut equation sequentially with infinite loop.
		local blinkTextModifier = emo.SquenceModifier(
			emo.AlphaModifier(1, 0, 1000, emo.easing.CubicIn),
			emo.AlphaModifier(0, 1, 1000, emo.easing.CubicOut));
		blinkTextModifier.setRepeatCount(-1); // -1 means infinite loop
		text.addModifier(blinkTextModifier);
		
		// onDrawFrame(dt) will be called 5 seconds later
		event.enableOnDrawCallback(5000);
    }

	/*
	 * Called when the app has gained focus
	 */
    function onGainedFocus() {
        print("Loading::onGainedFocus");
    }
    
	/*
	 * Called when the app has lost focus
	 */
    function onLostFocus() {
        print("Loading::onLostFocus"); 
    }

	/*
	 * Called when the class ends
	 */
    function onDispose() {
        print("Loading::onDispose");
        
        // remove sprite from the screen
        circle.remove();
		text.remove();
    }
    /*
     * Enabled after onDrawCalleback event is enabled by enableOnDrawCallback
     * dt parameter is a delta time (millisecond)
     */
    function onDrawFrame(dt) {
	
		// now loading is completed so proceed to the next stage.
		event.disableOnDrawCallback();
		stage.load(Main());
	}
}
class Main {
	text = emo.TextSprite("font_16x16.png",
		" !\"c*%#'{}@+,=./0123456789:;[|]?&ABCDEFGHIJKLMNOPQRSTUVWXYZ",
		16, 16, 2, 1);

	/*
	 * Called when this class is loaded
	 */
    function onLoad() {
        print("Main::onLoad");
		text.setText("CLICK TO START");
		text.moveCenter(stage.getWindowWidth()  / 2,
						stage.getWindowHeight() / 2);
		text.load();
    }

	/*
	 * Called when the app has gained focus
	 */
    function onGainedFocus() {
        print("Main::onGainedFocus");
    }

	/*
	 * Called when the app has lost focus
	 */
    function onLostFocus() {
        print("Main::onLostFocus"); 
    }

	/*
	 * Called when the class ends
	 */
    function onDispose() {
        print("Main::onDispose");
    }
	
	/*
	 * touch event
	 */
	function onMotionEvent(mevent) {
		if (mevent.getAction() == MOTION_EVENT_ACTION_DOWN) {
			text.setText("STARTED");
			text.moveCenter(stage.getWindowWidth()  / 2,
							stage.getWindowHeight() / 2);
		}
	}
}
function emo::onLoad() {
    stage.load(Loading());
}
