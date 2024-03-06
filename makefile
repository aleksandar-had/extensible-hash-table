CXX = g++
# CXXFLAGS = -Wall -g -fopenmp
CXXFLAGS = -Wall -fopenmp -O3
LXXFLAGS = -fopenmp

SRC_DIR = ./src
OBJ_DIR = ./obj
MAIN = main
OBJS =  $(OBJ_DIR)/main.o \
		$(OBJ_DIR)/lock_free_list.o \
		$(OBJ_DIR)/lock_free_hashtable.o \
		$(OBJ_DIR)/lock_based_hashtable.o

$(MAIN): $(OBJS)
	$(CXX) $(LXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

all: $(MAIN)

clean:
	rm $(OBJ_DIR)/*.o $(MAIN)

check:
	cppcheck $(SRC_DIR)/*cpp --language=c++ --enable=all --suppress=missingIncludeSystem