run: llauncher
	./llauncher
llauncher: json.c utils.c launcher.c
	clang -o llauncher json.c launcher.c utils.c -lcurl -fsanitize=address -g -Wextra -Wall
release: json.c utils.c launcher.c
	clang -O3 -march=native -flto -o llauncher-s4 json.c launcher.c utils.c -lcurl -Wextra -Wall
