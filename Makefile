all : vis_mem_analyzer vis_mem_plot

OPENCV_LIBS = -I/usr/local/include/opencv -I/usr/local/include -L/usr/local/lib -lopencv_cudabgsegm -lopencv_cudaobjdetect -lopencv_cudastereo -lopencv_shape -lopencv_stitching -lopencv_cudafeatures2d -lopencv_superres -lopencv_cudacodec -lopencv_videostab -lopencv_cudaoptflow -lopencv_cudalegacy -lopencv_calib3d -lopencv_features2d -lopencv_objdetect -lopencv_highgui -lopencv_videoio -lopencv_photo -lopencv_imgcodecs -lopencv_cudawarping -lopencv_cudaimgproc -lopencv_cudafilters -lopencv_video -lopencv_ml -lopencv_imgproc -lopencv_flann -lopencv_cudaarithm -lopencv_viz -lopencv_core -lopencv_cudev

SRCS = mem_analyser.cpp activity.cpp trace_session.cpp
PLOT_SRCS = trace_plot.cpp activity.cpp trace_session.cpp

FLAGS = -std=c++11 -lpthread

vis_mem_analyzer : $(SRCS)
	$(info Building memory analyzer)
	@(g++ $(SRCS) -o vis_mem_analyzer $(FLAGS) $(OPENCV_LIBS)) && echo "Build succeeded."

vis_mem_plot : $(PLOT_SRCS)
	$(info Building memory plotter)
	@(g++ $(PLOT_SRCS) -o vis_mem_plot $(FLAGS) $(OPENCV_LIBS)) && echo "Build succeeded."

clean :
	$(info cleaning build files)
	@rm -f vis_mem_analyzer vis_mem_plot