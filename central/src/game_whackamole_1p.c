#include <game.h>
#include <string.h>
#include <stdlib.h>

#define CHALLENGE_NUM_MAX 256
#define MAX_ROUNDS		  6
#define PERIPHERALS_MAX	  8

K_SEM_DEFINE(m_sem_num_players_update, 0, 1);
K_SEM_DEFINE(m_sem_peripheral_update, 0, 1);
K_SEM_DEFINE(m_sem_second_tick, 0, 1);

int num_players;
bool ping_received;

const led_effect_cfg_t led_effect_challenge = {.color1 = LED_COLOR_PURPLE, .color2 = LED_COLOR_ORANGE, .color_end = LED_COLOR_BLACK,
                                               .speed = 45, .num_repeats = LED_REPEAT_INFINITE};
const led_effect_cfg_t led_effect_result_good = {.color1 = LED_COLOR_GREEN, .color2 = LED_COLOR_BLACK, .color_end = LED_COLOR_BLACK,
                                               .speed = 60, .num_repeats = 4};
const led_effect_cfg_t led_effect_result_bad = {.color1 = LED_COLOR_RED, .color2 = LED_COLOR_BLACK, .color_end = LED_COLOR_BLACK,
                                               .speed = 60, .num_repeats = 4};
const led_effect_cfg_t led_effect_result_win  = {.color1 = LED_COLOR_GREEN, .color2 = LED_COLOR_BLACK, .color_end = LED_COLOR_BLACK,
                                               .speed = 30, .num_repeats = LED_REPEAT_INFINITE};
const led_effect_cfg_t led_effect_result_lose = {.color1 = LED_COLOR_RED, .color2 = LED_COLOR_BLACK, .color_end = LED_COLOR_BLACK,
                                               .speed = 8, .num_repeats = LED_REPEAT_INFINITE};

struct whackamole_t {
    int round_duration_s;
	int number_of_rounds;
	int current_round;
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
    uint32_t challenge_response_time_list[CHALLENGE_NUM_MAX];
    uint32_t challenge_counter;
    uint32_t challenge_average;
	int challenge_queued_by_peripheral[PERIPHERALS_MAX];
} player[1];

void game_timer_func(struct k_timer *timer_id); 

K_TIMER_DEFINE(m_timer_game, game_timer_func, NULL);

enum {PER_INDEX_ALL = 0x1000, PER_INDEX_ALL_P1, PER_INDEX_ALL_P2};

static void send_color_effect(struct game_t *game, uint32_t per_index, uint8_t sub_cmd, const led_effect_cfg_t *effect, uint16_t timeout)
{
    static uint8_t led_effect_cmd[LED_EFFECT_CMD_SIZE];
    led_effect_to_cmd_to(effect, sub_cmd, led_effect_cmd, timeout);
	if(per_index == PER_INDEX_ALL) {
         for(int i = 0; i < player[0].per_num; i++) {
            game->bt_send(i, led_effect_cmd, LED_EFFECT_CMD_SIZE);
			//k_msleep(2);
        }       
    }
    else {
        game->bt_send(per_index, led_effect_cmd, LED_EFFECT_CMD_SIZE);
    }
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
						player[0].score--;
						printk("Wrong button pressed (%i). -1 point. Score %i\n", bt_evt->con_index, player[0].score);
					}
				}
            }
            else if (memcmp(bt_evt->data, "TD:", 3) == 0 && bt_evt->data_len >= 9) {
                uint32_t response_time = 0;
				last_chg_response_time = k_uptime_get_32();
                for(int i = 0; i < 6; i++) {
                    response_time = response_time * 10 + (bt_evt->data[i+3] - '0');
                }
				//printk("Received from index %i, expected %i\n", bt_evt->con_index, player[0].chg_per_index);
				// Check if the right button was pressed
				if(bt_evt->con_index == player[0].chg_per_index) {
					if(response_time < whackamole.target_pr_round[whackamole.current_round]) {
						// Response time sufficient, award one point
						player[0].score++;
						printk("+1 point. Total score %i\n", player[0].score);
						send_color_effect(game, PER_INDEX_ALL, '0', &led_effect_result_good, 0);
					}
					else {
						// Too slow, no points awarded
						printk("Too slow. No points..\n");
						send_color_effect(game, PER_INDEX_ALL, '0', &led_effect_result_bad, 0);
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
				else {
					// Player pressed the wrong button. Subtract one point!
					player[0].score--;
					printk("Maybe wrong button pressed! -1 point. Total score %i\n", player[0].score);
				}
            }
			else if (memcmp(bt_evt->data, "TO", 2) == 0) {
				// Challenge timed out
				printk("Too slow. No points..\n");
				send_color_effect(game, PER_INDEX_ALL, '0', &led_effect_result_bad, 0);
			}
            k_sem_give(&m_sem_peripheral_update);
            break;
    }
}

static void whackamole_play(struct game_t *game)
{
    printk("Welcome to Whack-A-Mole! The most exiting Bluetooth game in the world!!!\n");
    printk("Waiting for peripherals to connect...\n");
    
	whackamole.target_pr_round[0] = 1500;
	whackamole.target_pr_round[1] = 1200;
	whackamole.target_pr_round[2] = 1000;
	whackamole.target_pr_round[3] = 800;
	whackamole.target_pr_round[4] = 550;
	whackamole.target_pr_round[5] = 300;
    whackamole.round_duration_s = 20 * 4;
    whackamole.time = 0;
    whackamole.challenge_int_min = 4 * 4;
    whackamole.challenge_int_range = 4 * 4;
    whackamole.chg_rsp_total = whackamole.chg_rsp_counter = 0;
	whackamole.game_running = false;

    // Waiting for players to connect
    ping_received = false;
    while (num_players < 1 || !ping_received) {
        if (k_sem_take(&m_sem_num_players_update, K_MSEC(100)) == 0) {
            printk("Controllers connected: %i\n", num_players);
        }
    }

	player[0].per_num = num_players;
	player[0].score = 0;
	player[0].chg_per_index = -1;

    // Use the uptime to provide a semi random seed to the random generator
    srand(k_uptime_get_32());

    printk("Starting new game\n");
    for(int i = 0; i < player[0].per_num; i++) {
        game->bt_send(i, "RST", 3);
    }

	whackamole.game_running = true;

    // Starting first round
    k_timer_start(&m_timer_game, K_MSEC(250), K_MSEC(250));

	for(whackamole.current_round = 0; whackamole.current_round < MAX_ROUNDS; whackamole.current_round++) {
		printk("Round %i starting...\n", (whackamole.current_round + 1));
		whackamole.time = 0;
		do {
			whackamole.time_until_challenge = whackamole.time + whackamole.challenge_int_min + rand() % whackamole.challenge_int_range;
			while(whackamole.time < whackamole.time_until_challenge) {
				k_sem_take(&m_sem_second_tick, K_FOREVER);
			}
			int random_peripheral_index = rand() % player[0].per_num;
			player[0].chg_per_index_previous = player[0].chg_per_index;
			player[0].chg_per_index = random_peripheral_index;
			printk("Sending to per num %i\n", random_peripheral_index);
			send_color_effect(game, random_peripheral_index, '2', &led_effect_challenge, 2000);
		} while(whackamole.time < whackamole.round_duration_s);
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
			printk("  Challenge %i: Time %i ms\n", (i+1), result);
			if (result < min) min = result;
			if (result > max) max = result;
			total += result; 
		}
		printk("  Best result:  %i ms\n", min);
		printk("  Worst result: %i ms\n", max);
		player[0].challenge_average = total / player[0].challenge_counter;
		printk("  Average:      %i ms\n", player[0].challenge_average);
	}

}

void game_timer_func(struct k_timer *timer_id)
{
    whackamole.time++;
    k_sem_give(&m_sem_second_tick);
}

int whackamole_init(struct game_t *game)
{
    game->play = whackamole_play;
    game->bt_rx = whackamole_bt_rx;
    num_players = 0;

    return 0;
}