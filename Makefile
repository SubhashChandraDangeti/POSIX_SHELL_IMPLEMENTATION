CXX = g++
CXXFLAGS = -Wall -g

TARGET = myshell

SRCS = Main.cpp \
       builtins/cd.cpp \
       builtins/pwd.cpp \
       builtins/echo.cpp \
       builtins/ls.cpp \
       builtins/pinfo.cpp \
       builtins/search.cpp \
       builtins/history.cpp \
       utils/helpers.cpp \
       system/system.cpp

OBJS = $(SRCS:.cpp=.o)

# Add readline and history libraries
LIBS = -lreadline -lhistory

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
