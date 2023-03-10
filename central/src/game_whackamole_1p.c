#include <game.h>
#include <string.h>
#include <stdlib.h>

#define CHALLENGE_NUM_MAX 256
#define MAX_ROUNDS		  6
#define PERIPHERALS_MAX	  8

K_SEM_DEFINE(m_sem_num_players_update, 0, 1);
K_SEM_DEFINE(m_sem_peripheral_update, 0, 1);
K_SEM_DEFINE(m_sem_second_tick, 0, 1);

struct game_t *this;

int num_players;
bool ping_received;

const led_effect_cfg_t led_effect_challenge = {.color1 = LED_COLOR_PURPLE, .color2 = LED_COLOR_ORANGE, .color_end = LED_COLOR_BLACK,
                                               .speed = 45, .num_repeats = LED_REPEAT_INFINITE};
const led_effect_cfg_t led_effect_result_good = {.color1 = LED_COLOR_GREEN, .color2 = LED_COLOR_BLACK, .color_end = LED_COLOR_BLACK,
                                               .speed = 60, .num_repeats = 3};
const led_effect_cfg_t led_effect_result_bad = {.color1 = LED_COLOR_RED, .color2 = LED_COLOR_BLACK, .color_end = LED_COLOR_BLACK,
                                               .speed = 60, .num_repeats = 3};
const led_effect_cfg_t led_effect_result_win  = {.color1 = LED_COLOR_GREEN, .color2 = LED_COLOR_BLACK, .color_end = LED_COLOR_BLACK,
                                               .speed = 30, .num_repeats = LED_REPEAT_INFINITE};
const led_effect_cfg_t led_effect_result_lose = {.color1 = LED_COLOR_RED, .color2 = LED_COLOR_BLACK, .color_end = LED_COLOR_BLACK,
                                               .speed = 8, .num_repeats = LED_REPEAT_INFINITE};
const led_effect_cfg_t led_effect_new_round   = {.color1 = LED_COLOR_BLACK, .color2 = LED_COLOR_WHITE, .color_end = LED_COLOR_BLACK,
                                               .speed = 30, .num_repeats = 2};

struct whackamole_t {
    int round_duration_s;
	int number_of_rounds;
	int current_round;
	int challenges_pr_round;
	int challenge_index;
	int target_pr_round[MAX_ROUNDS];
    int time, time_until_challenge;
    int challenge_int_min, challenge_int_range;
    int chg_rsp_total, chg_rsp_counter;
	bool game_running;
} whackamole;

struct player_t {
    uint32_t per_index;
    uint32_t per_num;
	int chg_per_index, chg_per_index_previous;
    bool player_ping_received;
    int time_until_challenge;
	int score;
	int missing_scores;
	int fouls;
    uint32_t challenge_response_time_list[CHALLENGE_NUM_MAX];
    uint32_t challenge_counter;
    uint32_t challenge_average;
	int challenge_queued_by_peripheral[PERIPHERALS_MAX];
} player[1];

void game_timer_func(struct k_timer *timer_id); 

K_TIMER_DEFINE(m_timer_game, game_timer_func, NULL);

enum {PER_INDEX_ALL = 0x1000, PER_INDEX_ALL_P1, PER_INDEX_ALL_P2};

static void send_color_effect(uint32_t per_index, uint8_t sub_cmd, const led_effect_cfg_t *effect, uint16_t timeout)
{
    static uint8_t led_effect_cmd[LED_EFFECT_CMD_SIZE];
    led_effect_to_cmd_to(effect, sub_cmd, led_effect_cmd, timeout);
	if(per_index == PER_INDEX_ALL) {
         for(int i = 0; i < player[0].per_num; i++) {
            this->bt_send(i, led_effect_cmd, LED_EFFECT_CMD_SIZE);
			//k_msleep(2);
        }       
    }
    else {
        this->bt_send(per_index, led_effect_cmd, LED_EFFECT_CMD_SIZE);
    }
}

static uint8_t per_cmd_buf[20];
static void send_per_cmd_chg_start(uint8_t p_index, uint16_t target_time)
{
	int i = 0;
	per_cmd_buf[i++] = 'A';
	per_cmd_buf[i++] = p_index;
	per_cmd_buf[i++] = (uint8_t)num_players;
	per_cmd_buf[i++] = (uint8_t)(target_time >> 8);
	per_cmd_buf[i++] = (uint8_t)target_time;
	this->bt_ctrl_send(per_cmd_buf, i);
}

static void send_per_cmd_chg_finish(uint8_t p_index, uint16_t time, uint16_t target, bool success, uint16_t points, uint16_t fouls)
{
	int i = 0;
	per_cmd_buf[i++] = 'B';
	per_cmd_buf[i++] = p_index;
	per_cmd_buf[i++] = (uint8_t)(time >> 8);
	per_cmd_buf[i++] = (uint8_t)time;
	per_cmd_buf[i++] = (uint8_t)(target >> 8);
	per_cmd_buf[i++] = (uint8_t)target;
	per_cmd_buf[i++] = success ? 1 : 0;
	per_cmd_buf[i++] = (uint8_t)(points >> 8);
	per_cmd_buf[i++] = (uint8_t)points;
	per_cmd_buf[i++] = (uint8_t)(fouls >> 8);
	per_cmd_buf[i++] = (uint8_t)fouls;
	this->bt_ctrl_send(per_cmd_buf, i);
}

static void send_per_cmd_round_start(int round_index, int round_total, int target_time)
{
	int i = 0;
	per_cmd_buf[i++] = 'C';
	per_cmd_buf[i++] = (uint8_t)round_index;
	per_cmd_buf[i++] = (uint8_t)round_total;
	per_cmd_buf[i++] = (uint8_t)num_players;
	per_cmd_buf[i++] = (uint8_t)(target_time >> 8);
	per_cmd_buf[i++] = (uint8_t)target_time;
	this->bt_ctrl_send(per_cmd_buf, i);
}

static void send_per_cmd_game_start()
{
	int i = 0;
	per_cmd_buf[i++] = 'D';
	this->bt_ctrl_send(per_cmd_buf, i);
}

static void send_per_cmd_game_finish(int score, int min, int max, int average)
{
	int i = 0;
	per_cmd_buf[i++] = 'E';
	per_cmd_buf[i++] = (uint8_t)(score >> 8);
	per_cmd_buf[i++] = (uint8_t)score;
	per_cmd_buf[i++] = (uint8_t)(min >> 8);
	per_cmd_buf[i++] = (uint8_t)min;
	per_cmd_buf[i++] = (uint8_t)(max >> 8);
	per_cmd_buf[i++] = (uint8_t)max;
	per_cmd_buf[i++] = (uint8_t)(average >> 8);
	per_cmd_buf[i++] = (uint8_t)average;
	this->bt_ctrl_send(per_cmd_buf, i);
}

static void send_per_cmd_num_con_change(int num_con)
{
	int i = 0;
	per_cmd_buf[i++] = 'F';
	per_cmd_buf[i++] = (uint8_t)num_con;
	this->bt_ctrl_send(per_cmd_buf, i);
}

#define CONSOLE_SCORE_PROGRESS_COL 50
static int console_print_progress = -1;
static int console_print_goal_line_index;
static bool challenge_pending = false;
static int foul_presses = 0;

void challenge_finalize(uint32_t time, uint32_t target_time, bool success, int pad_lines)
{
 	if(success) {
		// Response time sufficient, award one point
		player[0].score++;
		if(foul_presses > 0) {
			player[0].score -= foul_presses;
			player[0].fouls += foul_presses;
		}
		printk("+");
		int goal_line_tmp_index = pad_lines + console_print_goal_line_index - CONSOLE_SCORE_PROGRESS_COL - 1;
		for(int i = 0; i < pad_lines; i++) (i == goal_line_tmp_index) ? printk("|") : printk(" ");
		printk("+1 point. Score %i. Time %i ms\n", player[0].score, time);
		send_color_effect(PER_INDEX_ALL, '0', &led_effect_result_good, 0);
		send_per_cmd_chg_finish(0, (uint16_t)time, target_time, true, 1, foul_presses);
	}
	else {
		// Too slow, no points awarded
		printk("X");
		for(int i = 0; i < pad_lines; i++) printk(" ");
		if(foul_presses > 0) {
			player[0].score -= foul_presses;
			player[0].fouls += foul_presses;
			printk("Fouls: %i. Score %i", foul_presses, player[0].score);
		}
		else {
			printk("-");
		}
		printk("\n");
		send_color_effect(PER_INDEX_ALL, '0', &led_effect_result_bad, 0);
		send_per_cmd_chg_finish(0, (uint16_t)time, target_time, false, 0, foul_presses);
	}
	foul_presses = 0;
}

void whackamole_bt_rx(struct game_t *game, struct app_bt_evt_t *bt_evt)
{
	static uint32_t last_chg_response_time;
    switch(bt_evt->type) {
        case APP_BT_EVT_CON_NUM_CHANGE:
            num_players = bt_evt->num_connected;
            k_sem_give(&m_sem_num_players_update); 
            break;
        case APP_BT_EVT_RX_DATA:
            if (memcmp(bt_evt->data, "PING", 4) == 0) {
				ping_received = true;
				if(whackamole.game_running) {
					// If a ping happens very quickly after a valid response, assume it was a dummy double click and not the players fault
					uint32_t diff_time = k_uptime_get_32() - last_chg_response_time;
					if(diff_time > 400){
						//player[0].score--;
						foul_presses++;
						//printk("Wrong button pressed (%i). -1 point. Score %i\n", bt_evt->con_index, player[0].score);
					}
				}
            }
            else if (memcmp(bt_evt->data, "TD:", 3) == 0 && bt_evt->data_len >= 9) {
                uint32_t response_time = 0;
				last_chg_response_time = k_uptime_get_32();
                for(int i = 0; i < 6; i++) {
                    response_time = response_time * 10 + (bt_evt->data[i+3] - '0');
                }
				int pad_spaces_to_print = CONSOLE_SCORE_PROGRESS_COL - console_print_progress;
				console_print_progress = -1;
				challenge_pending = false;
				//printk("Received from index %i, expected %i\n", bt_evt->con_index, player[0].chg_per_index);

				if(response_time < whackamole.target_pr_round[whackamole.current_round]) {
					challenge_finalize(response_time, whackamole.target_pr_round[whackamole.current_round],
										true, pad_spaces_to_print);
				}
				else {
					challenge_finalize(response_time, whackamole.target_pr_round[whackamole.current_round],
										false, pad_spaces_to_print);
				}

				// Add the response time to the list
				if (player[0].challenge_counter < CHALLENGE_NUM_MAX) {
					player[0].challenge_response_time_list[player[0].challenge_counter++] = response_time;

					whackamole.chg_rsp_total += response_time;
					whackamole.chg_rsp_counter++;
				}
				// Clear the challenge peripheral index to avoid double presses giving double points
				player[0].chg_per_index = -1;
            }
			else if (memcmp(bt_evt->data, "TO", 2) == 0) {
				// Challenge timed out
				int pad_spaces_to_print = CONSOLE_SCORE_PROGRESS_COL - console_print_progress;
				console_print_progress = -1;
				challenge_pending = false;
				challenge_finalize(0, whackamole.target_pr_round[whackamole.current_round], false, pad_spaces_to_print);
			}
            k_sem_give(&m_sem_peripheral_update);
            break;
    }
}

#define TICKS_PR_SEC 20
#define TIMEOUT_BUFFER_MS 300

static void whackamole_play(struct game_t *game)
{
	k_msleep(1000);
    
	printk("Welcome to Whack-A-Mole! The most exiting Bluetooth game in the world!!!\n");
    printk("Waiting for peripherals to connect...\n");
    
	whackamole.target_pr_round[0] = 1200;
	whackamole.target_pr_round[1] = 1000;
	whackamole.target_pr_round[2] = 800;
	whackamole.target_pr_round[3] = 600;
	whackamole.target_pr_round[4] = 400;
	whackamole.target_pr_round[5] = 300;
    whackamole.round_duration_s = 20 * TICKS_PR_SEC;
    whackamole.time = 0;
    whackamole.challenge_int_min = 2 * TICKS_PR_SEC;
    whackamole.challenge_int_range = 3 * TICKS_PR_SEC / 2;
	whackamole.challenges_pr_round = 10;
    whackamole.chg_rsp_total = whackamole.chg_rsp_counter = 0;
	whackamole.game_running = false;

	while (1) {
		// Waiting for players to connect
		ping_received = false;
		while (num_players < 1 || !ping_received) {
			if (k_sem_take(&m_sem_num_players_update, K_MSEC(100)) == 0) {
				printk("\rControllers connected: %i   ", num_players);
				send_per_cmd_num_con_change(num_players);	
			}
		}

		player[0].per_num = num_players;
		player[0].score = 0;
		player[0].missing_scores = 0;
		player[0].fouls = 0;
		player[0].chg_per_index = -1;

		// Use the uptime to provide a semi random seed to the random generator
		srand(k_uptime_get_32());

		printk("\n\nStarting new game\n");
		for(int i = 0; i < player[0].per_num; i++) {
			game->bt_send(i, "RST", 3);
		}
		send_per_cmd_game_start();

		k_msleep(1000);

		whackamole.game_running = true;

		// Starting first round
		k_timer_start(&m_timer_game, K_MSEC(1000 / TICKS_PR_SEC), K_MSEC(1000 / TICKS_PR_SEC));

		for(whackamole.current_round = 0; whackamole.current_round < MAX_ROUNDS; whackamole.current_round++) {
			uint32_t target_time = whackamole.target_pr_round[whackamole.current_round];
			k_msleep(750);
			
			printk("\nRound %i starting... Respond quicker than %i ms!\n", (whackamole.current_round + 1), target_time);
			send_color_effect(PER_INDEX_ALL, '0', &led_effect_new_round, 0);
			send_per_cmd_round_start(whackamole.current_round, MAX_ROUNDS, target_time);

			console_print_goal_line_index = (target_time / 50) + 2;
			for (int i = 0; i < (console_print_goal_line_index - 1); i++) printk(" ");
			printk("|\n");

			k_msleep(750);
			
			whackamole.time = 0;
			whackamole.time_until_challenge = 20;
			whackamole.challenge_index = 0;
			do {			
				k_sem_take(&m_sem_second_tick, K_FOREVER);
				if (whackamole.time >= whackamole.time_until_challenge) {	
					// Start a new challenge
					int random_peripheral_index = rand() % player[0].per_num;
					player[0].chg_per_index_previous = player[0].chg_per_index;
					player[0].chg_per_index = random_peripheral_index;
					foul_presses = 0;
					challenge_pending = true;
					send_color_effect(random_peripheral_index, '2', &led_effect_challenge, target_time + TIMEOUT_BUFFER_MS);
					console_print_progress = true;
					whackamole.time_until_challenge = whackamole.time + (target_time + 200) / TICKS_PR_SEC + rand() % whackamole.challenge_int_range;
					send_per_cmd_chg_start(random_peripheral_index, target_time);
					whackamole.challenge_index++;
				}
			} while(whackamole.challenge_index < whackamole.challenges_pr_round || challenge_pending);
		}

		// Print an end of round report
		printk("Game complete!\n");
		k_timer_stop(&m_timer_game);
		whackamole.game_running = false;

		int result, min = 1000000000, max = 0, total = 0;
		if (player[0].challenge_counter > 0) {
			printk("\nResults: \n");
			for (int i = 0; i < player[0].challenge_counter; i++) {
				result = player[0].challenge_response_time_list[i];
				//printk("  Challenge %i: Time %i ms\n", (i+1), result);
				if (result < min) min = result;
				if (result > max) max = result;
				total += result; 
			}
			printk("  Total score:  %i points\n", player[0].score);
			printk("  Best result:  %i ms\n", min);
			printk("  Worst result: %i ms\n", max);
			player[0].challenge_average = total / player[0].challenge_counter;
			printk("  Average:      %i ms\n", player[0].challenge_average);
		}
		
		send_per_cmd_game_finish(player[0].score, min, max, player[0].challenge_average);

		printk("\nPress any button to start a new game\n");
	}

}

void game_timer_func(struct k_timer *timer_id)
{
    whackamole.time++;
    k_sem_give(&m_sem_second_tick);
	if(console_print_progress >= 0) {
		(console_print_progress == console_print_goal_line_index) ? printk("|") : printk("-");
		console_print_progress++;
	}
}

int whackamole_init(struct game_t *game)
{
	this = game;
    game->play = whackamole_play;
    game->bt_rx = whackamole_bt_rx;
    num_players = 0;

    return 0;
}