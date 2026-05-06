run: llauncher
	./llauncher
llauncher: json.c utils.c launcher.c
	clang -o llauncher json.c launcher.c utils.c -lcurl -fsanitize=address -g
