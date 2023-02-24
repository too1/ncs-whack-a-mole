#include <game.h>
#include <string.h>
#include <stdlib.h>

#define CHALLENGE_NUM_MAX 32

#define COLOR_PLAYER1       0x33FF11
#define COLOR_PLAYER2       0x2299EE

K_SEM_DEFINE(m_sem_num_players_update, 0, 1);
K_SEM_DEFINE(m_sem_peripheral_update, 0, 1);
K_SEM_DEFINE(m_sem_second_tick, 0, 1);

int num_players;
bool ping_received;


const led_effect_cfg_t led_effect_p1_select = {.color1 = COLOR_PLAYER1, .color2 = LED_COLOR_BLACK, .color_end = LED_COLOR_BLACK,
                                               .speed = 12, .num_repeats = LED_REPEAT_INFINITE};
const led_effect_cfg_t led_effect_p2_select = {.color1 = COLOR_PLAYER2, .color2 = LED_COLOR_BLACK, .color_end = LED_COLOR_BLACK,
                                               .speed = 12, .num_repeats = LED_REPEAT_INFINITE};
const led_effect_cfg_t led_effect_challenge = {.color1 = LED_COLOR_PURPLE, .color2 = LED_COLOR_ORANGE, .color_end = LED_COLOR_BLACK,
                                               .speed = 45, .num_repeats = LED_REPEAT_INFINITE};
const led_effect_cfg_t led_effect_result_good = {.color1 = LED_COLOR_GREEN, .color2 = LED_COLOR_BLACK, .color_end = LED_COLOR_BLACK,
                                               .speed = 60, .num_repeats = 40};
const led_effect_cfg_t led_effect_result_bad = {.color1 = LED_COLOR_RED, .color2 = LED_COLOR_BLACK, .color_end = LED_COLOR_BLACK,
                                               .speed = 60, .num_repeats = 40};

struct whackamole_t {
    int round_duration_s;
    int time;
    int challenge_int_min, challenge_int_range;
} whackamole;

struct player_t {
    uint32_t per_index;
    uint32_t per_num;
    bool player_ping_received;
    int time_until_challenge;
    uint32_t challenge_response_time_list[CHALLENGE_NUM_MAX];
    uint32_t challenge_counter;
} player[2];

void game_timer_func(struct k_timer *timer_id); 

K_TIMER_DEFINE(m_timer_game, game_timer_func, NULL);

enum {PER_INDEX_ALL = 0x1000, PER_INDEX_ALL_P1, PER_INDEX_ALL_P2};

static void send_color_effect(struct game_t *game, uint32_t per_index, uint8_t sub_cmd, const led_effect_cfg_t *effect)
{
    static uint8_t led_effect_cmd[LED_EFFECT_CMD_SIZE];
    led_effect_to_cmd(effect, sub_cmd, led_effect_cmd);
    if(per_index == PER_INDEX_ALL_P1) {
        for(int i = 0; i < player[0].per_num; i++) {
            game->bt_send(player[0].per_index + i, led_effect_cmd, LED_EFFECT_CMD_SIZE);
        }
    }
    else if(per_index == PER_INDEX_ALL_P2) {
        for(int i = 0; i < player[1].per_num; i++) {
            game->bt_send(player[1].per_index + i, led_effect_cmd, LED_EFFECT_CMD_SIZE);
        }
    }
    else if(per_index == PER_INDEX_ALL) {
         for(int i = 0; i < (player[0].per_num + player[1].per_num); i++) {
            game->bt_send(i, led_effect_cmd, LED_EFFECT_CMD_SIZE);
        }       
    }
    else {
        game->bt_send(per_index, led_effect_cmd, LED_EFFECT_CMD_SIZE);
    }
}

void whackamole_bt_rx(struct game_t *game, struct app_bt_evt_t *bt_evt)
{
    int player_index = (bt_evt->con_index < player[1].per_index) ? 0 : 1;
    switch(bt_evt->type) {
        case APP_BT_EVT_CON_NUM_CHANGE:
            num_players = bt_evt->num_connected;
            k_sem_give(&m_sem_num_players_update); 
            break;
        case APP_BT_EVT_RX_DATA:
            if (memcmp(bt_evt->data, "PING", 4) == 0) {
                ping_received = true;
                player[player_index].player_ping_received = true;
            }
            else if (memcmp(bt_evt->data, "TD:", 3) == 0 && bt_evt->data_len >= 9) {
                uint32_t response_time = 0;
                for(int i = 0; i < 6; i++) {
                    response_time = response_time * 10 + (bt_evt->data[i+3] - '0');
                }
                if (player[player_index].challenge_counter < CHALLENGE_NUM_MAX) {
                    player[player_index].challenge_response_time_list[player[player_index].challenge_counter++] = response_time;
                }
            }
            k_sem_give(&m_sem_peripheral_update);
            break;
    }
}

static void whackamole_play(struct game_t *game)
{
    printk("Welcome to Whack-A-Mole. The most exiting game in the world!!!\n");
    printk("Waiting for peripherals to connect...\n");
    
    whackamole.round_duration_s = 60;
    whackamole.time = 0;
    whackamole.challenge_int_min = 2;
    whackamole.challenge_int_range = 14;

    // Waiting for players to connect
    ping_received = false;
    while (num_players < 2 || !ping_received) {
        if (k_sem_take(&m_sem_num_players_update, K_MSEC(100)) == 0) {
            printk("Controllers connected: %i\n", num_players);
        }
    }

    // Signalling to players which each peripheral belongs to
    printk("Sufficient controllers connected. Proceeding to team selection...\n");
    for(int p = 0; p < 2; p++) {
        player[p].per_num = num_players / 2;
        player[p].per_index = p * (num_players / 2);
        player[p].player_ping_received = false;
        player[p].time_until_challenge = 0;
        player[p].challenge_counter = 0;
    }
    player[0].player_ping_received = player[1].player_ping_received = false;

    send_color_effect(game, PER_INDEX_ALL_P1, '0', &led_effect_p1_select);
    send_color_effect(game, PER_INDEX_ALL_P2, '0', &led_effect_p2_select);

    // Waiting for confirmation to proceed from both players
    while (!(player[0].player_ping_received && player[1].player_ping_received)) {
        k_sem_take(&m_sem_peripheral_update, K_FOREVER);
    }

    // Use the uptime to provide a semi random seed to the random generator
    srand(k_uptime_get_32());

    printk("Sending reset command to all peripherals\n");
    for(int i = 0; i < (player[0].per_num + player[1].per_num); i++) {
        game->bt_send(i, "RST", 3);
    }

    // Starting first round
    k_timer_start(&m_timer_game, K_SECONDS(1), K_SECONDS(1));

    while (whackamole.time < whackamole.round_duration_s) {
        k_sem_take(&m_sem_second_tick, K_FOREVER);
    
        // Waiting for a random time and triggering an update on a random peripheral for each team
        for (int p = 0; p < 2; p++) {
            if (player[p].time_until_challenge == 0) {
                player[p].time_until_challenge = whackamole.challenge_int_min + rand() % whackamole.challenge_int_range;
                printk("Player %i: Challenge in %i seconds\n", p, player[p].time_until_challenge);
            }
            else {
                player[p].time_until_challenge--;
                if (player[p].time_until_challenge == 0) {
                    int random_peripheral_index = player[p].per_index + rand()%player[p].per_num;
                    send_color_effect(game, random_peripheral_index, '1', &led_effect_challenge);
                }
            }
        }
    }

    // Print an end of round report
    printk("Round complete!\n");
    k_timer_stop(&m_timer_game);
    for (int p = 0; p < 2; p++) {
        int result, min = 1000000000, max = 0, total = 0;
        if (player[p].challenge_counter > 0) {
            printk("\nResults player %i: \n", p);
            for (int i = 0; i < player[p].challenge_counter; i++) {
                result = player[p].challenge_response_time_list[i];
                printk("  Challenge %i: Time %i ms\n", (i+1), result);
                if (result < min) min = result;
                if (result > max) max = result;
                total += result; 
            }
            printk("  Best result:  %i ms\n", min);
            printk("  Worst result: %i ms\n", max);
            printk("  Average:      %i ms\n", total / player[p].challenge_counter);
        }
        else {
            printk("\nNo results for player %i\n",p);
        }
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