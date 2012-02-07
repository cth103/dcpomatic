SRC :=		src/main_window.cc src/main.cc

dvdomatic:	$(SRC)
		g++ -g -Wall -o $@ $(SRC) `pkg-config --cflags --libs gtkmm-2.4`
