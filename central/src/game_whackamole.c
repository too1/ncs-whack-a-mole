#include <game.h>
#include <string.h>

K_SEM_DEFINE(m_sem_num_players_update, 0, 1);
K_SEM_DEFINE(m_sem_peripheral_update, 0, 1);
int num_players;
bool ping_received;

struct player_t {
    uint32_t per_index;
    uint32_t per_num;
    bool player_ping_received;
} player[2];

void whackamole_bt_rx(struct game_t *game, struct app_bt_evt_t *bt_evt)
{
    switch(bt_evt->type) {
        case APP_BT_EVT_CON_NUM_CHANGE:
            num_players = bt_evt->num_connected;
            k_sem_give(&m_sem_num_players_update); 
            break;
        case APP_BT_EVT_RX_DATA:
            if (memcmp(bt_evt->data, "PING", 4) == 0) {
                ping_received = true;
                if (bt_evt->con_index < player[1].per_index) {
                    player[0].player_ping_received = true;
                } else {
                    player[1].player_ping_received = true;
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

    // Waiting for players to connect
    ping_received = false;
    while (num_players < 2 || !ping_received) {
        if (k_sem_take(&m_sem_num_players_update, K_MSEC(100)) == 0) {
            printk("Controllers connected: %i\n", num_players);
        }
    }

    // Signalling to players which each peripheral belongs to
    printk("Sufficient controllers connected. Proceeding to team selection...\n");
    player[0].per_index = 0;
    player[0].per_num = 1;
    player[1].per_index = 1;
    player[1].per_num = 1;
    player[0].player_ping_received = player[1].player_ping_received = false;
    uint8_t bt_cmd[2] = "P0";
    for(int p = 0; p < 2; p++) {
        for(int i = 0; i < player[p].per_num; i++) {
            bt_cmd[1] = p + '0';
            game->bt_send(i + player[p].per_index, bt_cmd, 2);
            k_msleep(240);
        }
    }

    // Waiting for confirmation to proceed from both players
    while (!(player[0].player_ping_received && player[1].player_ping_received)) {
        k_sem_take(&m_sem_peripheral_update, K_FOREVER);
    }
    printk("Sending reset command to all peripherals\n");
    for(int p = 0; p < 2; p++) {
        for(int i = 0; i < player[p].per_num; i++) {
            game->bt_send(i + player[p].per_index, "RST", 3);
            k_msleep(240);
        }
    }

    // Starting first round

    // Waiting for a random time and triggering an update on a random peripheral for each team


}

int whackamole_init(struct game_t *game)
{
    game->play = whackamole_play;
    game->bt_rx = whackamole_bt_rx;
    num_players = 0;

    return 0;
}