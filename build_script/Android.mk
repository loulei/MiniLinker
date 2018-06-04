LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := minilinker
LOCAL_SRC_FILES := ../src/MiniLinker.c \
				   ../src/Loader.c \
				   ../src/mini_linker_phdr.c \
                   ../src/Parser.c
LOCAL_LDLIBS := -llog 

include $(BUILD_EXECUTABLE)


