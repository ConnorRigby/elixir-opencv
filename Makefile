ifeq ($(ERL_EI_INCLUDE_DIR),)
$(error ERL_EI_INCLUDE_DIR not set. Invoke via mix)
endif

CFLAGS += -fPIC -O2 -I$(ERL_EI_INCLUDE_DIR) 
LDFLAGS += -fPIC -shared -L$(ERL_EI_LIBDIR) -lopencv_core -lopencv_videoio

ifeq ($(MIX_TARGET),host)
CFLAGS += -I/usr/include/opencv4/
else
CFLAGS += $(ERL_CFLAGS)
LDFLAGS += $(ERL_LDFLAGS)
endif

.DEFAULT_GOAL: all
.PHONY: all clean

all: priv priv/erl_video_capture_nif.so priv/erl_mat_nif.so priv/erl_cv_nif.so

priv:
	mkdir -p priv

priv/erl_video_capture_nif.so: c_src/erl_cv_util.cpp c_src/erl_video_capture_nif.cpp c_src/queue.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) c_src/erl_cv_util.cpp c_src/erl_video_capture_nif.cpp  c_src/queue.cpp -o priv/erl_video_capture_nif.so

priv/erl_mat_nif.so: c_src/erl_cv_util.cpp c_src/erl_mat_nif.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) c_src/erl_cv_util.cpp c_src/erl_mat_nif.cpp -o priv/erl_mat_nif.so

priv/erl_cv_nif.so: c_src/erl_cv_util.cpp c_src/erl_cv_nif.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) c_src/erl_cv_util.cpp c_src/erl_cv_nif.cpp -o priv/erl_cv_nif.so

clean:
	$(RM) priv/erl_video_capture_nif.so priv/erl_mat_nif.so