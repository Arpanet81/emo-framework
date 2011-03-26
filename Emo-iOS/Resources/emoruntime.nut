/******************************************************
 *                                                    *
 *   RUNTIME CLASSES AND CONSTANTS FOR EMO-FRAMEWORK  *
 *                                                    *
 *            !!DO NOT EDIT THIS FILE!!               *
 ******************************************************/

OPT_ENABLE_PERSPECTIVE_NICEST   <- 0x1000;
OPT_ENABLE_PERSPECTIVE_FASTEST  <- 0x1001;
OPT_WINDOW_FORCE_FULLSCREEN     <- 0x1002;
OPT_WINDOW_KEEP_SCREEN_ON       <- 0x1003;

MOTION_EVENT_ACTION_DOWN         <- 0;
MOTION_EVENT_ACTION_UP           <- 1;
MOTION_EVENT_ACTION_MOVE         <- 2;
MOTION_EVENT_ACTION_CANCEL       <- 3;
MOTION_EVENT_ACTION_OUTSIDE      <- 4;
MOTION_EVENT_ACTION_POINTER_DOWN <- 5;
MOTION_EVENT_ACTION_POINTER_UP   <- 6;

KEY_EVENT_ACTION_DOWN            <- 0;
KEY_EVENT_ACTION_UP              <- 1;
KEY_EVENT_ACTION_MULTIPLE        <- 2;

META_NONE           <- 0;
META_ALT_ON         <- 0x02;
META_ALT_LEFT_ON    <- 0x10;
META_ALT_RIGHT_ON   <- 0x20;
META_SHIFT_ON       <- 0x01;
META_SHIFT_LEFT_ON  <- 0x40;
META_SHIFT_RIGHT_ON <- 0x80;
META_SYM_ON         <- 0x04;

SENSOR_TYPE_ACCELEROMETER      <- 1;
SENSOR_TYPE_MAGNETIC_FIELD     <- 2;
SENSOR_TYPE_GYROSCOPE          <- 4;
SENSOR_TYPE_LIGHT              <- 5;
SENSOR_TYPE_PROXIMITY          <- 8;

SENSOR_STATUS_UNRELIABLE       <- 0;
SENSOR_STATUS_ACCURACY_LOW     <- 1;
SENSOR_STATUS_ACCURACY_MEDIUM  <- 2;
SENSOR_STATUS_ACCURACY_HIGH    <- 3;

SENSOR_STANDARD_GRAVITY           <-  9.80665;
SENSOR_MAGNETIC_FIELD_EARTH_MAX   <-  60.0;
SENSOR_MAGNETIC_FIELD_EARTH_MIN   <-  30.0;

class emo.MotionEvent {
    param = null;
    function constructor(args) {
        param = args;
    }

    function getPointerId() { return param[0]; }
    function getAction()    { return param[1]; }
    function getX() { return param[2]; }
    function getY() { return param[3]; }
    function getEventTime() { return param[4] + (param[5] / 1000); }
    function getDeviceId()  { return param[6]; }
    function getSource() { return param[7]; }

    function toString() {
        local sb = "";
        for(local i = 0; i < param.len(); i++) {
            sb = sb + param[i] + " ";
        }
        return sb;
    }
}

class emo.KeyEvent {
    param = null;
    function constructor(args) {
        param = args;
    }
    function getAction() { return param[0]; }
    function getKeyCode() { return param[1]; }
    function getRepeatCount() { return param[2]; }
    function getMetaState() { return param[3]; }
    function getEventTime() { return param[4] + (param[5] / 1000); }
    function getDeviceId()  { return param[6]; }
    function getSource() { return param[7]; }

    function toString() {
        local sb = "";
        for(local i = 0; i < param.len(); i++) {
            sb = sb + param[i] + " ";
        }
        return sb;
    }
}

function _OnMotionEvent(...) {
    onMotionEvent(emo.MotionEvent(vargv));
}

function _OnKeyEvent(...) {
    onKeyEvent(emo.KeyEvent(vargv));
}

