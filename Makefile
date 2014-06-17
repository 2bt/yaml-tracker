CFLAG = -Iinclude
LFLAG = -lyaml-cpp -lsndfile -lSDL -lportmidi

SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:src/%.cpp=obj/%.o)
TARGET = yt


all: $(TARGET)


obj/%.o: src/%.cpp Makefile
	@mkdir -p $(@D)
	g++ --std=c++11 -Wall -O2 -c $< $(CFLAG) -o $@



$(TARGET): Makefile $(OBJ)
	g++ --std=c++11 -Wall $(OBJ) $(LFLAG) -o $@


log.mp3: log.wav
	avconv -i $< -b 192k $@


clean:
	rm -rf obj
	rm -f $(TARGET) log.wav log.mp3
