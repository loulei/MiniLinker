LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := minilinker
LOCAL_SRC_FILES := ../src/MiniLinker.c \
                   ../src/Parser.c
LOCAL_LDLIBS := -llog 

include $(BUILD_EXECUTABLE)


