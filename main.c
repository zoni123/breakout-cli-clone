#include <stdio.h>
#include <termios.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#define MAPX 20
#define MAPY 10
#define BRICKY (MAPY / 2)
#define DEFHP 1

typedef struct {
	int32_t x, y;
} coord_pair;

typedef struct {
	int32_t hp;
} cell;

typedef struct {
	int32_t size;
	coord_pair pos, vel;
} movable;

typedef struct {
	uint8_t exit;
	cell bricks[BRICKY][MAPX];
	movable *player, *ball;
} cycle_args;

struct termios term;

void reset() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

void set_mode() {
    struct termios raw;

    tcgetattr(STDIN_FILENO, &term);

    atexit(reset);
    raw = term;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, O_RDONLY | O_NONBLOCK);
}

void init_movable(movable *t) {
	t->size = 1;

	t->pos.x = MAPX / 2;
	t->pos.y = MAPY - 1;

	t->vel.x = 0;
	t->vel.y = 0;
}

void init_params(void *args) {
	cycle_args *cycle = (cycle_args *)args;
	init_movable(cycle->player);
	init_movable(cycle->ball);
	cycle->ball->vel.x = -1;
	cycle->ball->vel.y = -1;
	cycle->ball->pos.x--;
	cycle->ball->pos.y--;
	cycle->exit = 0;

	for (int32_t i = 0; i < BRICKY; i++) {
		for (int32_t j = 0; j < MAPX; j++) {
			cell predef = {DEFHP};
			cycle->bricks[i][j] = predef;
		}
	}
}

void draw_game(void *args) {
	cycle_args *cycle = (cycle_args *)args;
	for (int32_t i = 0; i < MAPY; i++) {
		for (int32_t j = 0; j < MAPX; j++) {
			if (i < BRICKY && cycle->bricks[i][j].hp > 0) {
				printf("-");
			} else if (i == cycle->ball->pos.y && j == cycle->ball->pos.x) {
				printf("O");
			} else if (i == cycle->player->pos.y && j <= cycle->player->pos.x + cycle->player->size && j >= cycle->player->pos.x - cycle->player->size) {
				printf("=");
			} else {
				printf(" ");
			}
		}
		printf("\n");
	}
}

void move(movable *t) {
	t->pos.x += t->vel.x;
	t->pos.y += t->vel.y;
}

void *default_cycle(void *args) {
	cycle_args *cycle = (cycle_args *)args;
	while (1) {
		printf("\033[H\n");

		if (cycle->exit) {
			return NULL;
		}

		if (cycle->ball->pos.x + cycle->ball->vel.x >= MAPX || cycle->ball->pos.x + cycle->ball->vel.x < 0) {
			cycle->ball->vel.x = -cycle->ball->vel.x;
		}

		if (cycle->ball->pos.y + cycle->ball->vel.y < 0) {
			cycle->ball->vel.y = -cycle->ball->vel.y;
		}

		if (cycle->ball->pos.y + cycle->ball->vel.y < BRICKY && cycle->bricks[cycle->ball->pos.y + cycle->ball->vel.y][cycle->ball->pos.x + cycle->ball->vel.x].hp > 0) {
			cycle->bricks[cycle->ball->pos.y + cycle->ball->vel.y][cycle->ball->pos.x + cycle->ball->vel.x].hp--;
			cycle->ball->vel.y = -cycle->ball->vel.y;
                }

		if (cycle->ball->pos.y + cycle->ball->vel.y == cycle->player->pos.y + cycle->player->vel.y && cycle->ball->pos.x + cycle->ball->vel.x <= cycle->player->pos.x + cycle->player->vel.x + cycle->player->size && cycle->ball->pos.x + cycle->ball->vel.x >= cycle->player->pos.x + cycle->player->vel.x - cycle->player->size) {
			cycle->ball->vel.y = -cycle->ball->vel.y;
		}

		move(cycle->ball);
		draw_game(args);
		usleep(200000);
	}
}

void read_key(void *args) {
	cycle_args *cycle = (cycle_args *)args;
	uint8_t c = 0;
	read(STDIN_FILENO, &c, 1);

	if (c == 'a' || c == 'A') {
		cycle->player->vel.x = -1;
	} else if (c == 'd' || c == 'D') {
		cycle->player->vel.x = 1;
	} else if (c == 27) {
		cycle->exit = 1;
	}

	if (cycle->exit) {
		return;
	}
	
	if (cycle->player->pos.x + cycle->player->vel.x + cycle->player->size < MAPX && cycle->player->pos.x + cycle->player->vel.x - cycle->player->size >= 0 && cycle->player->vel.x != 0) {
		printf("\033[H\n");
		move(cycle->player);
		draw_game(args);
	}

	cycle->player->vel.x = 0;
	usleep(10000);
}

int main(void) {
	movable player, ball;
	cycle_args cycle;
	cycle.player = &player;
        cycle.ball = &ball;
	pthread_t cycle_pt;
	uint8_t exit = 0;

	init_params(&cycle);
	set_mode();
	printf("\033[2J");

	pthread_create(&cycle_pt, NULL, default_cycle, &cycle);
	while (!cycle.exit) {
		read_key(&cycle);
	}

	pthread_join(cycle_pt, NULL);
	return 0;
}
