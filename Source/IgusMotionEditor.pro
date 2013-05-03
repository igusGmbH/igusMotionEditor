TEMPLATE = app
TARGET = IgusMotionEditor
QT += core \
    gui \
    xml \
    opengl
HEADERS += ResettableSlider.h \
    Joystick.h \
    JoystickControl.h \
    KeyframeEditor.h \
    globals.h \
    IgusMotionEditor.h \
    FlowLayout.h \
    Keyframe.h \
    KeyframeArea.h \
    KeyframePlayer.h \
    KeyframePlayerItem.h \
    RobotInterface.h \
    RobotView3D.h \
    Serial.h \
    JointConfiguration.h \
    ViewJoint.h
SOURCES += ResettableSlider.cpp \
    KeyframeEditor.cpp \
    IgusMotionEditor.cpp \
    FlowLayout.cpp \
    Keyframe.cpp \
    KeyframeArea.cpp \
    KeyframePlayer.cpp \
    KeyframePlayerItem.cpp \
    RobotInterface.cpp \
    RobotView3D.cpp \
    JoystickControl.cpp \
    JoystickWin.cpp \
    Serial.cpp \
    main.cpp \
    JointConfiguration.cpp \
    ViewJoint.cpp
win32:INCLUDEPATH += c:\\workspace\\libQGLViewer
win32:LIBS += -Lc:\\workspace\\libQGLViewer\\QGLViewer\\release \
    -lQGLViewer2 \
    -lWINMM
FORMS += KeyframeEditor.ui \
    igusmotioneditor.ui
RESOURCES += 
CONFIG += console

OTHER_FILES += \
    calibs/robot.ini \
    calibs/todo.txt \
    styles.css \
    release.bat \
    Changelog.txt
