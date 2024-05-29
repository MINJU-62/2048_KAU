#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>

#define max(a, b) ((a) > (b) ? (a) : (b))

#define NROWS 4
#define NCOLS NROWS

static const char *usage =
		"2048: A sliding tile puzzle game\n\n"
		"Usage: %s [-m M] [-r R] [-p P] [-s SEED] [-h]\n\n"
		"\t-m\tM\tGame mode M\n"
		"\t\t\t[1]: Normal mode\n"
		"\t\t\t[2]: Bomb mode (Start with a bomb tile 0 which moves but won't be combined) \n"
		"\t\t\t[3]: Chance mode (Start with a bomb tile 0 which moves but won't be combined) \n"
		"\t-r\tR\tRecord to file R\n"
		"\t-p\tP\tPlay back from file P\n"
		"\t-s\tSEED \tUse SEED for the random number generator\n"
		"\t-d\tDELAY\tDelay for DELAY ms when playing back\n"
		"\t-h\t\tShow this message\n";

typedef int tile;

struct game
{
	int turns, score;
	tile board[NROWS][NCOLS];
};

// 새로운 모드를 위한 타일 목록
typedef enum
{
	Number,
	Bomb,
	Chance
} TileType;

static FILE *recfile = NULL, *playfile = NULL;
static int batch_mode;
static int delay_ms = 250;

static struct timespec start_time; //시작 시간 저장 변수
static double elapsed_time = 0; //경과 시간 저장 변수


void record_achievement(int score)
{
	const char *achievement_file = "achievements.txt";
	FILE *record_achievement_file = fopen(achievement_file, "a");
	if (!record_achievement_file)
	{
		perror("Failed to open achievement file");
		return;
	}

	time_t t = time(NULL);
	struct tm *tm_info = localtime(&t);
	char date[20];
	strftime(date, sizeof(date), "%Y.%m.%d", tm_info);

	fprintf(record_achievement_file, "%d : %s\n", score, date); // 업적을 파일에 기록
	fclose(record_achievement_file);
}

void check_and_record_achievements(int score)
{
	int milestones[] = {1000, 2048, 5000, 10000};
	const int num_milestones = sizeof(milestones) / sizeof(milestones[0]);
	char buffer[256];
	FILE *record_achievement_file = fopen("achievements.txt", "r");
	if (record_achievement_file)
	{
		while (fgets(buffer, sizeof(buffer), record_achievement_file))
		{
			int recorded_score;
			sscanf(buffer, "%d", &recorded_score);
			for (int i = 0; i < num_milestones; i++)
			{
				if (recorded_score == milestones[i])
				{
					milestones[i] = -1;
				}
			}
		}
		fclose(record_achievement_file);
	}
	for (int i = 0; i < num_milestones; i++)
	{
		if (milestones[i] != -1 && score >= milestones[i])
		{
			record_achievement(milestones[i]);
		}
	}
}

// place_tile() returns 0 if it did place a tile and -1 if there is no open
// space.
int place_tile(struct game *game, TileType tile_type)
{
	// lboard is the "linear board" -- no need to distinguish rows/cols
	tile *lboard = (tile *)game->board;
	int i, num_zeros = 0;

	// Walk the board and count the number of empty tiles
	for (i = 0; i < NROWS * NCOLS; i++)
	{
		num_zeros += lboard[i] ? 0 : 1;
	}

	if (!num_zeros)
	{
		return -1;
	}

	// Choose the insertion point
	int loc = random() % num_zeros;

	// Find the insertion point and place the new tile
	for (i = 0; i < NROWS * NCOLS; i++)
	{
		if (!lboard[i] && !(loc--))
		{
			switch (tile_type)
			{
			case Number:
				lboard[i] = random() % 10 ? 1 : 2;
				return 0;
			case Bomb:
				lboard[i] = 15; // 폭탄 타일 (타일 넘버 15)
				return 0;
			case Chance:
				if (random() % 10 < 1)
				{
					lboard[i] = 2;
				}
				else if (random() % 10 == 9)
				{									// precentage of Chance: 1/10
					lboard[i] = 16; // 찬스 타일 (타일 넘버 16)
				}
				else
				{
					lboard[i] = 1;
				}
				return 0;
			}
		}
	}
	assert(0);
}

void print_tile(int tile)
{
	if (tile)
	{
		if (tile < 6)
		{
			attron(A_BOLD);
		}
		if (tile == 15) // 폭탄 타일 (타일 넘버 15)
		{
			int pair = COLOR_PAIR(7);
			attron(pair);
			attron(A_BOLD);
			printw("   X");
			attroff(pair);
		}
		else if (tile == 16) // 찬스 타일 (타일 넘버 16)
		{
			int pair = COLOR_PAIR(7);
			attron(pair);
			attron(A_BOLD);
			printw("   O");
			attroff(pair);
		}
		else
		{
			int pair = COLOR_PAIR(1 + (tile % 6));
			attron(pair);
			printw("%4d", 1 << tile);
			attroff(pair);
		}
		attroff(A_BOLD);
	}
	else
	{
		printw("   .");
	}
}

void print_game(const struct game *game)
{
	int r, c;
	move(0, 0);
	printw("Score: %6d  Turns: %4d", game->score, game->turns);

	struct timespec current_time;
	clock_gettime(CLOCK_MONOTONIC, &current_time); //현재 시간 얻어오기
	elapsed_time = (current_time.tv_sec - start_time.tv_sec) +
							(current_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;
	//현재 시간과 시작 시간을 비교해서 경과된 시간 계산
	mvprintw(1,0,"Time: %.2f seconds", elapsed_time); //curse 라이브러리에 있는 print함수.. 그냥 printf쓰면 출력 위치 오류로 엄청난 오>

	for (r = 0; r < NROWS; r++) {
		for (c = 0; c < NCOLS; c++) {
			move(r + 2, 5 * c);
			print_tile(game->board[r][c]);
		}
	}
  	refresh();
}

int combine_left(struct game *game, tile row[NCOLS])
{
	int c, did_combine = 0;
	for (c = 1; c < NCOLS; c++)
	{
		if (row[c])
		{
			if (row[c - 1] == 16 && row[c] == 16) // 찬스 타일과 찬스 타일이 만나면 2가 됨.
			{
				row[c - 1] = 2;
				row[c] = 0;
				game->score += 4; // 점수 계산
				did_combine = 1;
				printw("Combined 16 and 16 to 2, score: %d\n", game->score);
			}
			else if (row[c - 1] == row[c])
			{
				row[c - 1]++;
				row[c] = 0;
				game->score += (1 << row[c - 1]); // 점수 계산 (2^(row[c-1]))
				did_combine = 1;
				printw("Combined %d and %d to %d, score: %d\n", row[c - 1] - 1, row[c - 1] - 1, row[c - 1], game->score);
			}
			else if (row[c - 1] == 16 || row[c] == 16) // 찬스 타일과 숫자 타일이 만나면 무조건 합쳐짐
			{
				tile combined_num = row[c - 1] == 16 ? row[c] : row[c - 1];  // 숫자 타일
				row[c - 1] = combined_num + 1;
				row[c] = 0;
				game->score += (1 << (combined_num + 1)); // 점수 계산 (2^(combined_num + 1))
				did_combine = 1;
				printw("Combined chance and %d to %d, score: %d\n", combined_num, combined_num + 1, game->score);
			}
		}
	}
	check_and_record_achievements(game->score);
	return did_combine;
}

int deflate_left(tile row[NCOLS])
{
	tile buf[NCOLS] = {0};
	tile *out = buf;
	int did_deflate = 0;
	int in;

	for (in = 0; in < NCOLS; in++)
	{
		if (row[in] != 0)
		{
			*out++ = row[in];
			did_deflate |= buf[in] != row[in];
		}
	}

	memcpy(row, buf, sizeof(buf));
	return did_deflate;
}

void rotate_clockwise(struct game *game)
{
	tile buf[NROWS][NCOLS];
	memcpy(buf, game->board, sizeof(game->board));

	int r, c;
	for (r = 0; r < NROWS; r++)
	{
		for (c = 0; c < NCOLS; c++)
		{
			game->board[r][c] = buf[NCOLS - c - 1][r];
		}
	}
}

void move_left(struct game *game)
{
	int r, ret = 0;
	for (r = 0; r < NROWS; r++)
	{
		tile *row = &game->board[r][0];
		ret |= deflate_left(row);
		ret |= combine_left(game, row);
		ret |= deflate_left(row);
	}

	game->turns += ret;
}

void move_right(struct game *game)
{
	rotate_clockwise(game);
	rotate_clockwise(game);
	move_left(game);
	rotate_clockwise(game);
	rotate_clockwise(game);
}

void move_up(struct game *game)
{
	rotate_clockwise(game);
	rotate_clockwise(game);
	rotate_clockwise(game);
	move_left(game);
	rotate_clockwise(game);
}

void move_down(struct game *game)
{
	rotate_clockwise(game);
	move_left(game);
	rotate_clockwise(game);
	rotate_clockwise(game);
	rotate_clockwise(game);
}

// Pass by value because this function mutates the game
int lose_game(struct game test_game)
{
	int start_turns = test_game.turns;
	move_left(&test_game);
	move_up(&test_game);
	move_down(&test_game);
	move_right(&test_game);
	return test_game.turns == start_turns;
}

void init_curses()
{
	int bg = 0;
	initscr(); // curses init
	start_color();
	cbreak(); // curses don't wait for enter key
	noecho(); // curses don't echo the pressed key
	keypad(stdscr, TRUE);
	clear(); // curses clear screen and send cursor to (0,0)
	refresh();
	curs_set(0);

	bg = use_default_colors() == OK ? -1 : 0;
	init_pair(1, COLOR_WHITE, bg);
	init_pair(2, COLOR_GREEN, bg);
	init_pair(3, COLOR_YELLOW, bg);
	init_pair(4, COLOR_BLUE, bg);
	init_pair(5, COLOR_MAGENTA, bg);
	init_pair(6, COLOR_CYAN, bg);
	init_pair(7, COLOR_RED, bg);
}

int max_tile(const tile *lboard)
{
	int i, ret = 0;
	for (i = 0; i < NROWS * NCOLS; i++)
	{
		ret = max(ret, lboard[i]);
	}
	return ret;
}

FILE *fopen_or_die(const char *path, const char *mode)
{
	FILE *ret = fopen(path, mode);
	if (!ret)
	{
		perror(path);
		exit(EXIT_FAILURE);
	}
	return ret;
}

int get_input()
{
	if (playfile)
	{
		char *line = NULL;
		size_t len = 0;
		int ret = 'q';
		if (getline(&line, &len, playfile) > 0)
		{
			ret = line[strspn(line, " \t")];
		}
		free(line);
		if (!batch_mode)
			usleep(delay_ms * 1000);
		return ret;
	}
	else
	{
		return getch();
	}
}

void record(char key, const struct game *game)
{
	if (recfile)
	{
		fprintf(recfile, "%c:%d\n", key, game->score);
	}
}

int high_score = 0; // 최고 기록을 저장하는 전역 변수

const char *get_high_score_file_name(int game_mode)
{
	switch (game_mode)
	{
	case 2:
		return "high_score_bomb.txt";
	case 3:
		return "high_score_chance.txt";
	default:
		return "high_score_default.txt";
	}
}

void load_high_score(int game_mode)
{
	const char *file_name = get_high_score_file_name(game_mode);
	FILE *high_score_file = fopen(file_name, "r"); // 해당 파일을 읽기 모드로 열기
	if (high_score_file)
	{
		fscanf(high_score_file, "%d", &high_score); // 파일에서 최고 기록 읽어오기
		fclose(high_score_file);										// 파일 닫기
	}
}

void save_high_score(int game_mode, int score)
{
	const char *file_name = get_high_score_file_name(game_mode);
	FILE *high_score_file = fopen(file_name, "w"); // 해당 파일을 쓰기 모드로 열기
	if (high_score_file)
	{
		fprintf(high_score_file, "%d", score); // 파일에 최고 기록 쓰기
		fclose(high_score_file);							 // 파일 닫기
	}
}

int main(int argc, char **argv)
{
  const char *exit_msg = "";
  struct game game = {0};
	int last_turn = game.turns;

	time_t seed = time(NULL);
	int opt;
	int game_mode = 0;

	while ((opt = getopt(argc, argv, "hr:p:s:d:m:")) != -1)
	{
		switch (opt)
		{
		case 'm': // 게임 모드를 args로 받아와서 설정
			game_mode = atoi(optarg);
			break;
		case 'r':
			recfile = fopen_or_die(optarg, "w");
			break;
		case 'p':
			playfile = fopen_or_die(optarg, "r");
			break;
		case 's':
			seed = atoi(optarg);
			break;
		case 'd':
			delay_ms = atoi(optarg);
			break;
		case 'h':
			printf(usage, argv[0]);
			exit(EXIT_SUCCESS);
		default:
			fprintf(stderr, usage, argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	srandom(seed);
	load_high_score(game_mode);
  	clock_gettime(CLOCK_MONOTONIC, &start_time); // 게임이 시작하면 현재 시간을 얻어와서 저장.

	place_tile(&game, Number);
	if (game_mode == 2) // 폭탄 모드
	{
		place_tile(&game, Bomb);
	}
	place_tile(&game, Number);
	batch_mode = recfile && playfile;

	if (!batch_mode)
	{
		init_curses();
	}

	while (1)
	{
		if (!batch_mode)
		{
			print_game(&game);
		}

		if (lose_game(game)) 
    	{
			// Stop the timer when the game is lost
			struct timespec end_time;
			clock_gettime(CLOCK_MONOTONIC, &end_time);
			elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
							(end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;
			exit_msg = "lost";
			goto lose;
		}

		last_turn = game.turns;

		int key = get_input();
		switch (key)
		{
			case 'a':
			case KEY_LEFT:
				move_left(&game);
				break;
			case 's':
			case KEY_DOWN:
				move_down(&game);
				break;
			case 'w':
			case KEY_UP:
				move_up(&game);
				break;
			case 'd':
			case KEY_RIGHT:
				move_right(&game);
				break;
			case 'q':
				exit_msg = "quit";
				goto end;
		}

		if (last_turn != game.turns)
		{
			if (game_mode == 3) // 찬스 모드
			{
				place_tile(&game, Chance);
			}
			else
			{
				place_tile(&game, Number);
			}
			record(key, &game);

			if (game.score > high_score) // 최고 기록을 갱신했다면?
			{
				high_score = game.score;								// 최고 기록 갱신
				save_high_score(game_mode, high_score); // 해당 기록 파일에 저장
				if (!batch_mode)
				{
					move(8, 0);
					printw("New high score: %d\n", high_score); // 새로운 최고 기록 유저에게 알려주기
				}
			}
		}
	}

	lose:
		if (batch_mode)
		{
			return 0;
		}

		move(7, 0);
		printw("You lose! Press q to quit.");
		while (getch() != 'q');
	end:
		if (batch_mode)
		{
			return 0;
		}

	endwin();
	printf("You %s after scoring %d points in %d turns, "
				 "with largest tile %d\n",
				 exit_msg, game.score, game.turns,
				 1 << max_tile((tile *)game.board));

	if (game.score > high_score) // 게임 종료 시 최고 기록을 갱신했다면?
	{
		printf("Congratulations! New high score: %d\n", game.score); // 게임 종료 시 새로운 최고 기록 알림
	}
	else
	{
		printf("High score: %d\n", high_score); // 최고 기록 출력
	}
  	printf("Time played: %.2f seconds\n", elapsed_time); //진행 시간 출력
	return 0;
}