NAME = Matt_daemon
SRC = main.cpp
OBJ = $(SRC:.cpp=.o)
CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(OBJ) $(NAME)

re: fclean all

push:
	git add .
	@echo "Enter your commit message:"
	@read -r msg; git commit -m "$$msg"
	git push