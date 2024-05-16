/* Clone of the 2048 sliding tile puzzle game. (C) Wes Waugh 2014
 *
 * This program only works on Unix-like systems with curses. Link against
 * the curses library. You can pass the -lcurses flag to gcc at compile
 * time.
 *
 * This program is free software, licensed under the GPLv3. Check the
 * LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>

#define max(a, b) ((a) > (b) ? (a) : (b)) // 두 값 중 큰 값을 반환하는 매크로 함수

#define NROWS 4
#define NCOLS NROWS // ROW와 COL를 4로 정의

static const char *usage =
	"2048: A sliding tile puzzle game\n\n"
	"Usage: %s [-r R] [-p P] [-s SEED] [-h]\n\n"
	"\t-r\tR\tRecord to file R\n"
	"\t-p\tP\tPlay back from file P\n"
	"\t-s\tSEED \tUse SEED for the random number generator\n"
	"\t-d\tDELAY\tDelay for DELAY ms when playing back\n"
	"\t-h\t\tShow this message\n";

typedef int tile;

struct game {
	int turns, score;
	tile board[NROWS][NCOLS];
};

static FILE *recfile = NULL, *playfile = NULL;
static int batch_mode;
static int delay_ms = 250;

// 게임판에 새로운 타일을 놓는 함수
int place_tile(struct game *game) 
{
	tile *lboard = (tile *)game->board; // 4 * 4 보드판 정의
	int i, num_zeros = 0;

	// 타일에 빈 칸이 있는지 for문을 돌면서 확인
	for (i = 0; i < NROWS * NCOLS; i++) {
		num_zeros += lboard[i] ? 0 : 1; // lboard가 0이 아니면 num_zeros는 0이 증가, 아니면 1 증가
	}

	if (!num_zeros) { // num_zeros == 0 일 때, 즉 빈 칸이 없을 때
		return -1; // -1을 반환
	}

	// 빈 칸이 있을 때.
	int loc = random() % num_zeros; // 0부터 num_ zeros -1 까지 랜덤한 정수를 생성 , 빈 타일의 위치를 나타냄

	//
	for (i = 0; i < NROWS * NCOLS; i++) {
		if (!lboard[i] && !(loc--)) { // lboard가 0이고(비어있고), loc이 0 일 때
			lboard[i] = random() % 10 ? 1 : 2; // 0~9의 랜덤한 정수 생성, 10%의 확률로 1을 반환, 90%의 확률로 2를 반환
			return 0;
		}
	}
	assert(0);
}

void print_tile(int tile)
{
	if (tile) { // tile이 0이 아닌 경우에만 코드 블록을 실행
		if (tile < 6)
			attron(A_BOLD); // curses 라이브러리의 attron 함수.
	// tile이 6보다 작다면 볼드체로 출력, 2048 게임의 경우 2의 거듭제곱으로만 표현, 2와 4가 해당됨.		
		int pair = COLOR_PAIR(1 + (tile % 6)); // COLOR_PAIR gkatnsms 1~6에 해당하는 색상 쌍을 반환
		attron(pair); // 색상 쌍을 활성화
	/* 	init_pair(1, COLOR_RED, bg);
		init_pair(2, COLOR_GREEN, bg);
		init_pair(3, COLOR_YELLOW, bg);
		init_pair(4, COLOR_BLUE, bg);
		init_pair(5, COLOR_MAGENTA, bg);
		init_pair(6, COLOR_CYAN, bg); */
		printw("%4d", 1 << tile); // 타일 값을 화면에 출력, 비트 연산자를 사용해 2^tile 으로 계산
		attroff(pair);
		attroff(A_BOLD);
	}
	else {
		printw("   ."); // 빈 타일을 나타내는 . 으로 출력
	}
}

void print_game(const struct game *game) 
{
	int r, c; // row 와 col을 의미
	move(0, 0); // curses 라이브러리의 move 함수. 커서를 맨 왼쪽 상단(0,0) 위치로 이동시킴
	printw("Score: %6d  Turns: %4d", game->score, game->turns); // Score, Turns 출력
	for (r = 0; r < NROWS; r++) { // 보드판 2중 for문으로 순회
		for (c = 0; c < NCOLS; c++) {
			move(r + 2, 5 * c); // Score와 Turns가 출력되는 곳과 행으로 2칸 여백을 두고 타일 간 간격을 5칸으로 설정함
			print_tile(game->board[r][c]);
		}
	}

	refresh();
}

int combine_left(struct game *game, tile row[NCOLS]) // 행을 인자로 받음
{
	int c, did_combine = 0;
	for (c = 1; c < NCOLS; c++) { // 한 행의 모든 열을 순회
		if (row[c] && row[c-1] == row[c]) { // 타일이 비어있지 않고, 이전 열의 타일과 값이 같은지 확인
			row[c-1]++; // 같다면 인접 타일 값을 1 증가
			row[c] = 0; // 현재 타일을 비워줌
			game->score += 1 << (row[c-1] - 1); // 게임의 점수를 업데이트, 2의 거듭제곱을 계산하는 비트연산자
			did_combine = 1; // 합쳐졌다면 1로 설정
		}
	}
	return did_combine;
}

// deflate_left() returns nonzero if it did deflate, and 0 otherwise
int deflate_left(tile row[NCOLS]) // 행을 인자로 받음
{
	tile buf[NCOLS] = {0};
	tile *out = buf;
	int did_deflate = 0;
	int in;

	for (in = 0; in < NCOLS; in++) {
		if (row[in] != 0) { // 타일이 비어있지 않으면 코드 블록을 실행
			*out++ = row[in]; // 현재 타일을 buf 배열의 out이 가리키는 위치에 복사 후 out 포인터를 다음 위치로 이동시킴
			did_deflate |= buf[in] != row[in]; // 둘의 값이 다르면 did_deflate에 1 대입
			// 1이 되었다는건 타일이 밀기가 일어났다는 뜻.
		}
	}

	memcpy(row, buf, sizeof(buf)); // buf 배열의 내용을 row 배열에 복사
	return did_deflate; // deflate 되었는지 여부를 반환
}

void rotate_clockwise(struct game *game)
{
	tile buf[NROWS][NCOLS]; // 4 * 4 2차원 배열 선언
	memcpy(buf, game->board, sizeof(game->board)); // game->board배열을 buf에 복사

	int r, c; // row, col을 의미
	for (r = 0; r < NROWS; r++) {
		for (c = 0; c < NCOLS; c++) { // 보드판 순회
			game->board[r][c] = buf[NCOLS - c - 1][r]; // buf배열에서 시계방향으로 90도 회전시킨 배열을 game->board에 저장
		}
	}
} // rotate_clockwise 함수를 통해 move_left 함수 하나를 통해 4방향 이동이 가능함.

void move_left(struct game *game)
{
	int r, ret = 0;
	for (r = 0; r < NROWS; r++) { // 모든 행을 순회
		tile *row = &game->board[r][0]; // row 포인터는 현재 행의 첫 번째 타일 주소 할당
		ret |= deflate_left(row); // deflate가 발생했다면 1 반환
		ret |= combine_left(game, row); // 인접한 같은 값의 타일을 합침
		ret |= deflate_left(row); // 타일 합치기 이후 빈 칸이 발생할 수 있어서 deflate
	}

	game->turns += ret; // deflate 또는 combine이 발생했다면 turns를 1 증가
}

void move_right(struct game *game) 
{
	rotate_clockwise(game); // 두 번 회전시켜 move_left 진행
	rotate_clockwise(game);
	move_left(game);
	rotate_clockwise(game); // 두 번 회전시켜 원상복구
	rotate_clockwise(game);
}

// move_up과 move_down도 동일하게 작동함

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
	int start_turns = test_game.turns; // 게임이 시작되기 전 turns 값을 저장
	move_left(&test_game); // 이동했다면 turns ++
	move_up(&test_game); // 동일
	move_down(&test_game); // 동일
	move_right(&test_game); // 동일
	return test_game.turns == start_turns; // 4방향으로 이동해도 tunrs가 변하지 않았다면 lose, 0 반환
}

void init_curses()
{
	int bg = 0;
	initscr(); // curses init
	start_color();
	cbreak(); // curses don't wait for enter key
	noecho(); // curses don't echo the pressed key
	keypad(stdscr,TRUE);
	clear(); // curses clear screen and send cursor to (0,0)
	refresh();
	curs_set(0);

	bg = use_default_colors() == OK ? -1 : 0;
	init_pair(1, COLOR_RED, bg);
	init_pair(2, COLOR_GREEN, bg);
	init_pair(3, COLOR_YELLOW, bg);
	init_pair(4, COLOR_BLUE, bg);
	init_pair(5, COLOR_MAGENTA, bg);
	init_pair(6, COLOR_CYAN, bg);
}

int max_tile(const tile *lboard)
{
	int i, ret = 0;
	for (i = 0; i < NROWS * NCOLS; i++) {
		ret = max(ret, lboard[i]);
	}
	return ret; // lboard 배열 내에 가장 큰 타일의 값이 저장되어있음.
}

FILE *fopen_or_die(const char *path, const char *mode) // path와 mode는 파일 경로와 파일 열기 모드를 나타냄
{
	FILE *ret = fopen(path, mode); // 파일을 열 수 있으면 반환, 없으면 NULL 반환
	if (!ret) { // NULL이면
		perror(path); // 오류 메세지 출력 후 
		exit(EXIT_FAILURE); // 프로그램 비정상 종료
	}
	return ret; // 성공적으로 열었으면 ret 반환
}

int get_input()
{
	if (playfile) { // playfile이 NULL이 아닌 경우
		char *line = NULL; // 문자열 저장하기 위한 포인터 변수
		size_t len = 0; // 문자열의 길이를 저장하기 위한 변수
		int ret = 'q';
		if (getline(&line, &len, playfile) > 0) { // getline 함수는 성공적으로 문자열을 읽어오면 문자열 길이를 반환, 그렇지 않다면 -1 반환
			ret = line[strspn(line, " \t")]; // 맨 앞의 공백과 탭을 제거하고 실제 입력 값을 ret 변수에 저장
		}
		free(line);
		if (!batch_mode) // 자동 진행 모드인 경우
			usleep(delay_ms * 1000); // 각 턴 사이에 텀을 둠. 1000ms
		return ret; // 변수에 저장된 ASCII 코드 반환
	}
	else {
		return getch(); // playfile이 NULL이면 터미널에서 입력된 문자의 ASCII 코드 반환
	}
}

void record(char key, const struct game *game)
{
	if (recfile) { // NULL이 아닌 경우, 즉 게임 진행 정보를 기록할 파일이 열려있는 경우 코드 블록 실행, recfile은 게임 진행 정보를 기록할 파일을 가리키는 파일 포인터
		fprintf(recfile, "%c:%d\n", key, game->score);
	}
}

int main(int argc, char **argv)
{
	const char *exit_msg = ""; // 게임 종료 시 출력할 메세지 저장, 초기값은 빈 문자열
	struct game game = {0}; // game 구조체 선언, 모든 멤버변수 0으로 초기화
	int last_turn = game.turns; // turns를 0으로 만듦
	time_t seed = time(NULL); 
	int opt;

	while ((opt = getopt(argc, argv, "hr:p:s:d:")) != -1) {
		switch (opt) {
		case 'r': // 게임 정보를 기록할 파일 이름
			recfile = fopen_or_die(optarg, "w");
			break;
		case 'p': // 게임 정보를 읽어올 파일 이름
			playfile = fopen_or_die(optarg, "r");
			break;
		case 's': // 난수 생성기의 시드 값, 새로운 타일이 놓이는 위치와 타일의 값이 달라짐. 게임의 무작위성
			seed = atoi(optarg);
			break;
		case 'd': // 자동 진행 모드에서 지연시간
			delay_ms = atoi(optarg);
			break;
		case 'h': // 도움말 출력
			printf(usage, argv[0]);
			exit(EXIT_SUCCESS);
		default:
			fprintf(stderr, usage, argv[0]);
			exit(EXIT_FAILURE);
		}
	}
// ./2048_game -r game_record.txt -p game_replay.txt -s 1234 -d 500

	srandom(seed); // 난수 초기화
	place_tile(&game);
	place_tile(&game);
	batch_mode = recfile && playfile; // 두 파일이 모두 존재하는 경우 자동 진행 모드로 설정됨

	if (!batch_mode) {
		init_curses();
	}

	while (1) {
		if (!batch_mode) {
			print_game(&game);
		}

		if (lose_game(game)) {
			exit_msg = "lost"; 
			goto lose; // lose 라벨로 이동, line 346
		}

		last_turn = game.turns;

		int key = get_input();
		switch (key) { // 상하좌우로 움직이는 부분
		case 'a': case KEY_LEFT: move_left(&game); break; 
		case 's': case KEY_DOWN: move_down(&game); break;
		case 'w': case KEY_UP: move_up(&game); break;
		case 'd': case KEY_RIGHT: move_right(&game); break;
		case 'q':
			exit_msg = "quit";
			goto end; // end 라벨로 이동, line 354
		}

		if (last_turn != game.turns) { // 턴이 변경되었다면 새로운 타일을 놓고 게임 정보를 기록
			place_tile(&game);
			record(key, &game);
		}
	}

lose:
	if (batch_mode) { // 자동 진행 모드인 경우 바로 프로그램 종료
		return 0;
	}

	move(7, 0);
	printw("You lose! Press q to quit.");
	while (getch() != 'q');
end:
	if (batch_mode) { // 자동 진행 모드인 경우 바로 프로그램 종료
		return 0;
	}

	endwin(); // curses 모드 종료
	printf("You %s after scoring %d points in %d turns, "
		"with largest tile %d\n",
		exit_msg, game.score, game.turns,
		1 << max_tile((tile *)game.board));
	return 0;
}

