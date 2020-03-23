#define _WITH_DPRINTF
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "handler_string.h"

enum how_sort
{
    inc, /* in asceding order*/
    dec  /* in descending order */
};

enum type_list
{
    buy,
    sell
};

enum sess_states
{
    st_hello,
    st_wait,
    st_play,
    st_end_turn,
    st_finish
};

int level_change[5][5] =
{
    { 4, 4, 2, 1, 1 },
    { 3, 4, 3, 1, 1 },
    { 1, 3, 4, 3, 1 },
    { 1, 1, 3, 4, 3 },
    { 1, 1, 2, 4, 4 }
};

int level_raw[5][2] =     /* sell bank */
{
    {2, 800},
    {3, 650},
    {4, 500},
    {5, 400},
    {6, 300}
};

int level_product[5][2] =    /* buy bank */
{
    {6, 6500},
    {5, 6000},
    {4, 5500},
    {3, 5000},
    {2, 4500}
};

struct request_fabric
{
    char time_build;
    struct request_fabric *next;
};

struct request_auction
{
    int items;
    int price;
};

struct auction_list
{
    struct session *sess;
    struct request_auction req;
    enum type_list type;
    struct auction_list *next;
};

struct session
{
    int fd;
    int len_buf;
    int fabrics;
    int money;
    int raw;
    int product;
    int orders_produce;
    int id;
    char buf[1024];
    struct request_fabric *req_fabric;
    struct request_auction buy;
    struct request_auction sell;
    enum sess_states state;
};

struct list
{
    struct session *sess;
    struct list *next;
};

struct global_info
{
    int clients;
    int game_now;
    int max_clients;
    int level_market;
    int current_month;
    struct list *cl_lst;
};

void del_request_fabric(struct request_fabric *req_fabric)
{
    if (req_fabric)
    {
        del_request_fabric(req_fabric->next);
        free(req_fabric);
    }
}

void send_string(struct session *sess,  char *str)
{
    write(sess->fd, str, str_len(str));
}

void send_number(struct session *sess, int number)
{
    char *str = int_to_str(number);
    send_string(sess, str);
    free(str);
}

void send_string_all(struct list *cl_lst, char *str)
{
    if (cl_lst)
    {
        send_string(cl_lst->sess, str);
        send_string_all(cl_lst->next, str);
    }
}

void send_number_all(struct list *cl_lst, int number)
{
    if (cl_lst)
    {
        send_number(cl_lst->sess, number);
        send_number_all(cl_lst->next, number);
    }
}

void del_auction_list(struct auction_list *auct_lst)
{
    if (auct_lst)
    {
        del_auction_list(auct_lst->next);
        free(auct_lst);
    }
}

void print_lst(struct list *client_lst)
{
    if (client_lst)
    {
        printf("%d->", (client_lst->sess)->fd);
        print_lst(client_lst->next);
        return;
    }
    printf("NULL\n");
}

void print_lst_a(struct auction_list *auct_lst)
{
    if (auct_lst)
    {
        printf("%d %d->", (auct_lst->req).price, (auct_lst->req).items);
        print_lst_a(auct_lst->next);
        return;
    }
    printf("NULL\n");
}

void del_extra_clients(struct global_info *gi)
{
    struct list **p = &(gi->cl_lst);
    while(p && *p)
    {
        if ((*p)->sess->state == st_finish)
        {
            struct list *tmp = (*p)->next;
            free(*p);
            *p = tmp;
            (gi->clients)--;
            continue;
        }
        p = &((*p)->next);
    }
}

void add_request_fabric(struct session *sess,  char **word_v)
{
    int i, arg_f;
    str_to_int(word_v[1], &arg_f);
    for(i = 0; i < arg_f; i++)
    {
        if (sess->money >= 2500)
        {
            struct request_fabric *new;
            new = malloc(sizeof(struct request_fabric));
            new->time_build = 5;
            sess->money -= 2500;
            new->next = sess->req_fabric;
            sess->req_fabric = new;
            break;
        }
        send_string(sess, "You don't have enough money \n");
    }
}

char is_cmd_build_or_prod( char **word_v)
{
    char tmp;
    int arg_f;
    if (lenArgV(word_v) == 2)
    {
        tmp = str_to_int(word_v[1], &arg_f);
        if (tmp == 0 && arg_f > 0)
            return 1;
    }
    printf("IS CMD BUILD OR PROD  SAY NO \n");
    return 0;
}

char is_cmd_buy_or_sell( char **word_v)
{
    int items, price, tmp1, tmp2;
    if (lenArgV(word_v) == 3)
    {
        tmp1 = str_to_int(word_v[1], &items);
        tmp2 = str_to_int(word_v[2], &price);
        if (!tmp1 && !tmp2 && items > 0 && price > 0)
            return 1;
    }
    return 0;
}

void add_request_auction_buy(struct session *sess, char **word_v,
                                             struct global_info *gi)
{
    int items, price;
    int level = gi->level_market;
    str_to_int(word_v[1], &items);
    str_to_int(word_v[2], &price);
    if (items*price > sess->money)
    {
        send_string(sess, "Not enough money\n");
        return;
    }
    if (price < level_raw[level-1][1])
    {
        send_string(sess, "The price is less than acceptable\n");
        send_string(sess, "The min price is ");
        send_number(sess, level_raw[level][1]);
        send_string(sess, "\n");
        return;
    }
    (sess->buy).items = items;
    (sess->buy).price = price;
    printf("TEST %d %d\n", (sess->buy).items, (sess->buy).price);
    send_string(sess, "The request is accepted\n");
}

void add_request_auction_sell(struct session *sess, char **word_v,
                                             struct global_info *gi)
{
    int items, price;
    int level = gi->level_market;
    str_to_int(word_v[1], &items);
    str_to_int(word_v[2], &price);
    if (items > sess->product)
    {
        send_string(sess, "Not enough product\n");
        return;
    }
    if (price > level_product[level-1][1])
    {
        send_string(sess, "The price is more than acceptable\n");
        send_string(sess, "The max price is ");
        send_number(sess, level_product[level][1]);
        return;
    }
    (sess->sell).items = items;
    (sess->sell).price = price;
    send_string(sess, "The request is accepted\n");
}

void handler_st_hello(struct session *sess, struct global_info *gi)
{
    send_string(sess, "Number of connected users: ");
    send_number(sess, gi->clients);
    send_string(sess, "\nNumber users for start game: ");
    send_number(sess, gi->max_clients);
    send_string(sess, "\n");
    sess->state = st_wait;
}

void send_info_market(struct session *sess, struct global_info *gi)
{
    int clients = gi->clients;
    int level = gi->level_market;
    send_string(sess, "Active players: ");
    send_number(sess, clients);
    send_string(sess,"\nBank sells: \nitems: ");
    send_number(sess, (int)level_raw[level-1][0]*clients/2);
    send_string(sess, "\n min_price: ");
    send_number(sess, level_raw[level-1][1]);
    send_string(sess, "\nBank buys: \nitems:  ");
    send_number(sess, (int)level_product[level-1][0]*clients/2);
    send_string(sess, "\n max_price: ");
    send_number(sess, (int)level_product[level-1][1]);
    send_string(sess, "\n");
}

void say_left_client_all(struct global_info *gi)
{
    struct list *cl_lst = gi->cl_lst;
    send_string_all(cl_lst, "Number of connected users: ");
    send_number_all(cl_lst, gi->clients);
    send_string_all(cl_lst, "Number users for start game: ");
    send_number_all(cl_lst, gi->max_clients);
}

void send_req_auct(struct session *sess, struct request_auction req)
{
    if (req.price != -1 && req.items != -1)
    {
        dprintf(sess->fd, "price %d\n", req.price);
        dprintf(sess->fd, "items %d\n", req.items);
        return;
    }
    dprintf(sess->fd, "NULL\n");
}

void send_req_fabr(struct session *sess, struct request_fabric *req)
{
    int time;
    if (req)
    {
        time = req->time_build;
        dprintf(sess->fd, "\nRequerement for build fabric \n");
        dprintf(sess->fd, "still %d month\n", time);
        send_req_fabr(sess, req->next);
    }
}

void send_stat_act_clients(struct session *sess,
                                        struct global_info *gi)
{
    struct list *cl_lst = gi->cl_lst;
    int count = 0;
    while(cl_lst)
    {
        struct session *sess1 = cl_lst->sess;
        if (sess->state == st_play)
        {
            cl_lst = cl_lst->next;
            continue;
        }
        dprintf(sess->fd, "\nActive player id is: %d", sess1->id);
        count++;
        cl_lst = cl_lst->next;
    }
    dprintf(sess->fd, "\n Count active players : %d\n", count);
}

void send_status_clients(struct session *sess, struct global_info *gi)
{
    struct list *cl_lst = gi->cl_lst;
    while(cl_lst)
    {
        struct session *sess1 = cl_lst->sess;
        dprintf(sess->fd, "\nPlayer id: %d", sess1->id);
        dprintf(sess->fd, "\nfabrics: %d", sess1->fabrics);
        dprintf(sess->fd, "\nmoney %d", sess1->money);
        dprintf(sess->fd, "\nproduct %d",sess1->product);
        dprintf(sess->fd, "\norders produce %d",sess1->orders_produce);
        dprintf(sess->fd, "\nraw %d", sess1->raw);
        dprintf(sess->fd, "\nRequerement for buy product \n");
        send_req_auct(sess, sess1->buy);
        dprintf(sess->fd, "\nRequerement for sell product \n");
        send_req_auct(sess, sess1->sell);
        send_req_fabr(sess, sess1->req_fabric);
        dprintf(sess->fd, "\n");
        cl_lst = cl_lst->next;
    }
}

void send_info_help(struct session *sess)
{
    dprintf(sess->fd, "market - situation\n status -"
                        "info about all players\n"
                        "active - info about online players\n"
                        "help is help\n"
                        "build <arg> - build <arg> fabrics\n"
                        "prod <arg> - produce <arg> product\n"
                        "turn  - make end turn\n"
                        "buy <arg1> <arg2> buy <arg1> raw by"
                        "price <arg2>\n"
                        "sell <arg1> <arg2> sell <arg1> product"
                        "buy price <arg2>\n");
}

char handler_st_end_turn(struct session *sess,  char **word_v,
                                            struct global_info *gi)
{
    if (str_equal(word_v[0], "help") && lenArgV(word_v) == 1)
    {
        send_info_help(sess);
        return 1;
    }
    if (str_equal(word_v[0], "market") && lenArgV(word_v) == 1)
    {
        send_info_market(sess, gi);
        return 1;
    }
    if (str_equal(word_v[0], "status") && lenArgV(word_v) == 1)
    {
        send_status_clients(sess, gi);
        return 1;
    }
    if (str_equal(word_v[0], "active") && lenArgV(word_v) == 1)
    {
        send_stat_act_clients(sess, gi);
        return 1;
    }
    send_string(sess, "Bad input\n Please try again \n");
    return 0;
}

void add_request_produce(struct session *sess, char **word_v)
{
    int arg_f;
    str_to_int(word_v[1], &arg_f);
    printf("%d \n", arg_f);
    if (arg_f > sess->fabrics || arg_f > sess->raw)
    {
        send_string(sess, "value of orders produce is so much\n");
        return;
    }
    sess->orders_produce += arg_f;
}

void handler_st_play(struct session *sess,  char **word_v,
                                        struct global_info *gi)
{
    if (str_equal(word_v[0], "build") && is_cmd_build_or_prod(word_v))
        add_request_fabric(sess, word_v);
    else
    if (str_equal(word_v[0], "prod") && is_cmd_build_or_prod(word_v))
        add_request_produce(sess, word_v);
    else
    if (str_equal(word_v[0], "turn") && lenArgV(word_v) == 1)
    {
        send_string(sess, "You can use commands:"
                            " market, help, active\n");
        sess->state = st_end_turn;
    }
    else
    if (str_equal(word_v[0], "buy") && is_cmd_buy_or_sell(word_v))
        add_request_auction_buy(sess, word_v, gi);
    else
    if (str_equal(word_v[0], "sell") && is_cmd_buy_or_sell(word_v))
        add_request_auction_sell(sess, word_v, gi);
    else
        handler_st_end_turn(sess, word_v, gi);
}

char is_bankrupt(struct session *sess, struct global_info *gi)
{
    if (sess->money < 0)
    {
        send_string(sess, "You are bankrupt\n");
        sess->state = st_finish;
        del_extra_clients(gi);
        close(sess->fd);
        return 1;
    }
    return 0;
}

void next_step_session(struct session *sess, char **word_v,
                    struct global_info *gi)
{
    switch(sess->state)
    {
    case st_hello:
        handler_st_hello(sess, gi);
        break;
    case st_wait:
        send_string(sess, "The game is OFF \n");
        break;
    case st_play:
        handler_st_play(sess, word_v, gi);
        break;
    case st_end_turn:
        handler_st_end_turn(sess, word_v, gi);
        break;
    case st_finish:
        break;
    }
}

void init_request_auction(struct request_auction *req)
{
    req->price = -1;
    req->items = -1;
}

struct session* start_new_session(int fd, struct global_info *gi)
{
    struct session *sess;
    sess = malloc(sizeof(struct session));
    sess->fd = fd;
    sess->len_buf = 0;
    sess->state = st_hello;
    sess->fabrics = 2;
    sess->raw = 4;
    sess->product = 2;
    sess->money = 10000;
    sess->orders_produce = 0;
    sess->req_fabric = NULL;
    sess->id = gi->clients;
    init_request_auction(&(sess->buy));
    printf("%d %d\n", (sess->buy).price, (sess->buy).items);
    init_request_auction(&(sess->sell));
    send_string(sess, "Hello\n");
    dprintf(sess->fd, "Your id is: %d\n", sess->id);
    next_step_session(sess, NULL, gi);
    return sess;
}

/* is end of session now ? */
int try_to_read_data(struct session *sess)
{
    int rc, len = sess->len_buf;
    char *pbuf = sess->buf;
    rc = read(sess->fd, pbuf + len,  1024 - len);
    if (rc == -1)
    {
        perror("read");
        exit(1);
    }
    if (rc == 0)
        return 1;
    if (sess->len_buf + rc >=  1024)
    {
        send_string(sess, "Bad input\n Please try again \n");
        sess->len_buf = 0;
        return 1;
    }
    sess->len_buf += rc;
    return 0;
}


void finish_session(struct session *sess, struct global_info *gi)
{
    close(sess->fd);
    sess->state = st_finish;
    del_extra_clients(gi);
    say_left_client_all(gi);
}

void process_data(struct session *sess, struct global_info *gi)
{
    int prev_len = sess->len_buf;
    int pos, len_cmd, end_of_session;
    char *command, *buf = sess->buf;
    char **word_v;
    end_of_session = try_to_read_data(sess);
    if (end_of_session)
    {
        finish_session(sess, gi);
        return;
    }
    for(;;)
    {
        pos = find_char_in(buf + prev_len, sess->len_buf, '\n');
        if (pos == -1)          /* if \n not found  */
            return;
        len_cmd = prev_len + pos + 1;
        str_move(buf, buf + len_cmd, sess->len_buf - len_cmd);
        sess->len_buf -= len_cmd;
        command = create_str(buf + prev_len, prev_len + pos);
        word_v = make_argv(command);
        next_step_session(sess, word_v, gi);
        free(word_v);
    }
}

struct global_info* init_global_info(int number_clients)
{
    struct global_info *gl_inf;
    gl_inf = malloc(sizeof(struct global_info));
    gl_inf->clients = 0;
    gl_inf->game_now = 0;
    gl_inf->max_clients = number_clients;
    gl_inf->level_market = 3;
    gl_inf->cl_lst = NULL;
    return gl_inf;
}

struct list* add_client(int ls, struct global_info *gi)
{
    int new_fd;
    struct list *new_client;
    struct list *lst;
    lst = gi->cl_lst;
    new_fd = accept(ls, NULL, NULL);
    new_client = malloc(sizeof(struct list));
    new_client->sess = start_new_session(new_fd, gi);
    new_client->next = lst;
    return new_client;
}

int make_new_listen_socket(int port)
{
    int listener;
    struct sockaddr_in addr;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1)
    {
        perror("socket");
        exit(1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        perror("bind");
        exit(1);
    }
    if (listen(listener, 16))
    {
        perror("listen");
        exit(1);
    }
    return listener;
}


void start_game(struct global_info *gi)
{
    struct list *client_lst = gi->cl_lst;
    while(client_lst)
    {
        send_string(client_lst->sess, "The game is ON\n ");
        client_lst->sess->state = st_play;
        client_lst = client_lst->next;
    }
}

void meet_client(int ls, struct global_info *gi)
{
    int kick_fd;
    static char msg [] = "The game is ON, you will kick\n";
    if (!(gi->game_now))
    {
        send_string_all(gi->cl_lst, "New client!\n");
        send_string_all(gi->cl_lst, "Number of connected users: ");
        send_number_all(gi->cl_lst, gi->clients + 1);
        send_string_all(gi->cl_lst, "\n");
        (gi->clients)++;
        gi->cl_lst = add_client(ls, gi);
        return;
    }
    kick_fd = accept(ls, NULL, NULL);
    write(kick_fd, &msg, str_len(msg));
    close(kick_fd);
}

struct request_fabric* del_first(struct request_fabric *req_fabric)
{
    struct request_fabric **first = &(req_fabric);
    if (*first)
    {
        struct request_fabric *new_first = (*first)->next;
        free(*first);
        return new_first;
    }
    return *first;
}

void check_request_fabric(struct session *sess,
                                         struct global_info *gi)
{
    struct request_fabric **req = &(sess->req_fabric);
    while(*req)
    {
        ((*req)->time_build)--;
        if ((*req)->time_build == 0)
        {
            (sess->fabrics)++;
            sess->money -= 2500;
            dprintf(sess->fd, "Fabric built, taxe is: %d", 2500);
            if (is_bankrupt(sess, gi))
                return;
            (*req) = del_first(*req);
        }
        req = &((*req)->next);
    }
}

void check_all_request_fabric(struct global_info *gi)
{
    struct list *cl_lst = gi->cl_lst;
    while(cl_lst)
    {
        check_request_fabric(cl_lst->sess, gi);
        cl_lst = cl_lst->next;
    }
}

int min(int a, int b)
{
    return (a > b)? a : b;
}

void execute_request_produce(struct global_info *gi)
{
    int ord;
    struct list *cl_lst = gi->cl_lst;
    while(cl_lst)
    {
        struct session *sess = cl_lst->sess;
        ord = sess->orders_produce;
        if ((sess->money)*ord < 2000*ord || (sess->raw)*ord < ord)
            is_bankrupt(sess, gi);
        sess->money -= 2000*ord;
        sess->orders_produce = 0;
        sess->raw -= ord;
        dprintf(sess->fd, "Taxes for produce product:\n");
        dprintf(sess->fd, "money: %d", ord*2000);
        dprintf(sess->fd, "raw: %d", ord);
        cl_lst = cl_lst->next;
    }
}

void collect_taxes_for_storage_res(struct global_info *gi)
{
    struct list *cl_lst = gi->cl_lst;
    while(cl_lst)
    {
        struct session *sess = cl_lst->sess;
        int taxes;
        int fd = sess->fd;
        taxes = 300*(sess->raw) + 500*(sess->product) +
                    1000*(sess->fabrics);

        sess->money -= taxes;
        dprintf(fd, "\ntaxes resourses and fabrics: %d\n", taxes);
        is_bankrupt(sess, gi);
        cl_lst = cl_lst->next;
    }
}

char is_end_turn(struct list *cl_lst)
{
    char end_turn = 1;
    while(cl_lst)
    {
        if (cl_lst->sess->state != st_end_turn)
            return 0;
        cl_lst = cl_lst->next;
    }
    return end_turn;
}

int count_clients(struct list *cl_lst)
{
    int count = 0;
    while(cl_lst)
    {
        count++;
        cl_lst = cl_lst->next;
    }
    return count;
}

void change_level_market(struct global_info *gi)
{
    int level = gi->level_market;
    int r = 1 + (int)(12.0*rand()/(RAND_MAX + 1.0));
    int i = 0;
    while(r >= 0)
    {
        r = r - level_change[level-1][i];
        i++;
    }
    gi->level_market = i;
}

struct auction_list *init_auct_lst()
{
    struct auction_list *auct_lst;
    auct_lst = malloc(sizeof(struct auction_list));
    auct_lst->type = sell;
    init_request_auction(&(auct_lst->req));
    return auct_lst;
}

void push(struct auction_list **head)
{
    struct auction_list *tmp = init_auct_lst();
    tmp->next = *head;
    *head = tmp;
}

void init_req(struct request_auction *req, int price, int items)
{
    req->price = price;
    req->items = items;
}

struct auction_list *make_lst_auct(struct list *cl_lst,
                                                enum type_list type)
{
    struct auction_list *head = NULL;
    while(cl_lst)
    {
        struct session *sess;
        sess = cl_lst->sess;
        push(&head);
        (head)->type = type;
        head->sess = sess;
        printf("CL_LST\n");
        print_lst(cl_lst);
        cl_lst = cl_lst->next;
        if (type == buy && (sess->buy).price != -1)
        {
            (head)->sess = sess;
            printf("BUY TEST %d \n", (sess->buy).items);
            ((head)->req).price = (sess->buy).price;
            ((head)->req).items = (sess->buy).items;
            init_req((&(head)->req),(sess->buy).price,(sess->buy).items);
        }
        else
        if (type == sell && (sess->sell).price != -1)
        {
            (head)->sess = sess;
            printf("SELL TEST %d \n", (sess->sell).items);
            ((head)->req).price = (sess->sell).price;
            ((head)->req).items = (sess->sell).items;
            init_req((&(head)->req),(sess->sell).price,(sess->sell).items);

        }
    }
    return head;
}

int len_auct_list(struct auction_list *auct_lst)
{
    int len = 0;
    while(auct_lst)
    {
        len++;
        auct_lst = auct_lst->next;
    }
    return len;
}
/* fst is first. Need swap or not  */
char is_wrong_order(struct auction_list *fst,
                    struct auction_list *sec, enum how_sort sort)
{
    int pr1 = (fst->req).price;
    int pr2 = (sec->req).price;
    /* inc is in ascending order, dec is in descending order */
    return (sort == inc && pr1 > pr2) || (sort == dec && pr1 < pr2);
}

void swap_numbers(int *first, int *second)
{
    int for_swp;
    for_swp = *second;
    *second = *first;
    *first = for_swp;
}

void swap_sessions(struct session **first, struct session **second)
{
    struct session *for_swp;

    for_swp = *second;
    *second = *first;
    *first = for_swp;

}

void swap_requests(struct auction_list *fst, struct auction_list *sec)
{

    swap_sessions(&(fst->sess), &(sec->sess));
    swap_numbers(&((fst->req).price), &((sec->req).price));
    swap_numbers(&((fst->req).items), &((sec->req).items));

}

struct auction_list *sort_lst_auct(struct auction_list *auct_lst)
{

    int len = len_auct_list(auct_lst);
    int i = len, j = 0;
    enum how_sort sort;
    if (len == 0)
        return NULL;

    sort = (auct_lst->type == buy) ? dec : inc;

     /* bubble sort */

    for(i = len - 1; i >= 0; i--) /* was len, j = 1 */
        for(j = 0; j < i; j++)
            if (is_wrong_order(auct_lst, auct_lst->next, sort))
                swap_requests(auct_lst, auct_lst->next);

    return auct_lst;
}

int count_best_items(struct auction_list *auct_lst)
{
    int count = 0;
    int price;
    if (auct_lst == NULL)
        return count;
    price = (auct_lst->req).price;
    while(auct_lst)
    {
        if ((auct_lst->req).price == price)
        {
            count = count + (auct_lst->req).items;
            auct_lst = auct_lst->next;
            continue;
        }
        break;
    }
    return count;
}

/* suppose auct_lst is sorted */
/* execute the most profit requests for bank */
void send_info_about_req_auct(struct global_info *gi, int items,
                                struct auction_list *auct_lst)
{
    if (auct_lst)
    {
        send_string_all(gi->cl_lst, "Player ");
        send_number_all(gi->cl_lst, auct_lst->sess->id);
        send_string_all(gi->cl_lst, "\n items:");
        send_number_all(gi->cl_lst,  items);
        send_string_all(gi->cl_lst, "\n");
    }
}

void exec_req_auct(struct global_info *gi,
                            struct auction_list *auct_lst, int items)
{
        struct session *sess;
        struct request_auction *req;
        enum type_list type = auct_lst->type;
        sess = auct_lst->sess;
        req = &(auct_lst->req);
        if (type == sell)
        {
            sess->money += items*(req->price);
            sess->product -= items;
        }
        if (type == buy)
        {
            sess->money -= items*(req->price);
            sess->raw += items;
        }
        req->items -= items;
        send_info_about_req_auct(gi, items, auct_lst);
        if (req->items == 0)
            init_request_auction(req);
        if (req->items < 0)
            printf("bad input, exec req_auct\n");
    }
}

/* suppose that auct_lst was sorted */
void exec_profit_request(struct global_info *gi,
                            struct auction_list *auct_lst)
{
    int max_price = (auct_lst->req).price;
    printf("SELLLLLLL \n");
    while(auct_lst)
    {
        int items = ((auct_lst)->req).items;
        printf("EXEC PROFIT REQ: %d %d", items, max_price);
        if (((auct_lst)->req).price == max_price)
        {
            exec_req_auct(gi, auct_lst, items);
            send_info_about_req_auct(gi, items, auct_lst);
            auct_lst = auct_lst->next;
        }
        break;
    }
}

/* we delete in begin auct_list type most profit requests */
struct auction_list* del_extra_requests(struct auction_list *auct_lst)
{
    struct auction_list **p = &(auct_lst);
    int price;
    if (*p == NULL)
        return NULL ;
    price = ((*p)->req).price;
    while(*p && ((*p)->req).price == price)
    {
        struct auction_list *tmp = (*p)->next;
        free(*p);
        *p = tmp;
        continue;
    }
    return *p;
}

void write_zeroes_in(int *lst_winners, int len)
{
    int i;
    for(i = 0; i < len; i++)
        lst_winners[i] = 0;
}

int count_number_in_array(int *array, int len, int number)
{
    int i;
    int count = 0;
    for(i = 0; i < len; i++)
    {
        if (array[i] == number)
            count++;
    }
    return count;
}

int* choose_winners_auct(int len)
{
    int *lst_winners = malloc((len+1)*sizeof(int));
    int new_winner;
    int j = 0;
    write_zeroes_in(lst_winners, len);
    while(count_number_in_array(lst_winners, len, 0))
    {
        new_winner = 1 + (int)(rand()/(RAND_MAX + 1.0));
        lst_winners[j] = new_winner;
        j++;
    }
    lst_winners[len] = 0;
    return lst_winners;
}

int find_int_in_str(int *str, int search)
{
    int i = 0;
    while(str[i])
    {
        if (str[i] == search)
            return i;
        i++;
    }
    return -1;
}

void exec_req_winner_auct(struct global_info *gi,
                            struct auction_list *auct_lst,
                            int *lst_win, int bank_items)
{
    int i = 0;
    int pos;
    int items = 0;
    while(auct_lst && bank_items > 0)
    {
        items = (auct_lst->req).items;
        pos = find_int_in_str(lst_win, i);
        if (pos == -1)
        {
            i++;
            auct_lst = auct_lst->next;
            continue;
        }
        printf("EXEC REQ WINNEER %d\n", items);
        exec_req_auct(gi, auct_lst, min(bank_items, items));
        bank_items -= min(bank_items, items);
        auct_lst = auct_lst->next;
        i++;
    }
}

int count_best_requests(struct auction_list *auct_lst)
{
    int count = 0;
    int price = (auct_lst->req).price;
    while(auct_lst && price == (auct_lst->req).price)
    {
        count++;
        auct_lst = auct_lst->next;
    }
    return count;
}

void make_auction(struct global_info *gi, enum type_list type)
{
    int cl_items = 1, bank_items = 0;
    int lvl = gi->level_market, cl = gi->clients;
    int number_winners = 0, *lst_winners = NULL;
    struct auction_list *auct_lst = NULL;

    printf("CLIENTS LEVEL %d %d ",cl, lvl);
    if (type == buy)
        bank_items = (int)(level_product[lvl-1][0])*(cl/2);
    else
        bank_items = (int)(level_raw[lvl-1][0])*(cl/2);
    printf("AUCTION\n");
    auct_lst = make_lst_auct(gi->cl_lst, type);
    print_lst_a(auct_lst);
    printf("AUCTION SORT\n");
    auct_lst = sort_lst_auct(auct_lst);
    printf("AFTER SORT\n");
    print_lst_a(auct_lst);
    printf("BANK ITEMS %d", bank_items);
    while(auct_lst && bank_items > 0)
    {
        cl_items = count_best_items(auct_lst);
        printf("CL ITEMS BANK ITEMS %d %d\n", cl_items, bank_items);
        if (cl_items <= 0)
            return;
        if (cl_items <= bank_items)
        {
            printf("I AM HERE \n");
            exec_profit_request(gi, auct_lst);
            auct_lst = del_extra_requests(auct_lst);
            bank_items -= cl_items;
            break;
        }
        printf("I AM HERE 1\n");
        number_winners = count_best_requests(auct_lst);
        lst_winners = choose_winners_auct(number_winners);
        exec_req_winner_auct(gi, auct_lst, lst_winners, bank_items);
        del_auction_list(auct_lst);
        free(lst_winners);
        bank_items = 0;
    }
}

void init_all_request_auction(struct global_info *gi)
{
    struct list *cl_lst = gi->cl_lst;
    while(cl_lst)
    {
        init_request_auction(&(cl_lst->sess->buy));
        init_request_auction(&(cl_lst->sess->sell));
        cl_lst = cl_lst->next;
    }
}

void inform_global_events(struct global_info *gi)
{
    struct list *cl_lst;
    cl_lst = gi->cl_lst;
    gi->clients = count_clients(cl_lst);
    if (!(gi->game_now) && gi->clients == gi->max_clients)
    {
        gi->game_now = 1;
        start_game(gi);
        return;
    }
    if (cl_lst && is_end_turn(cl_lst))
    {
        collect_taxes_for_storage_res(gi);
        execute_request_produce(gi);
        (gi->current_month)++;
        change_level_market(gi);
        printf("AUCTION\n");
        send_string_all(cl_lst, "\nBank sells:\n");
        make_auction(gi, buy); /* clients buy raw */
        send_string_all(cl_lst, "\nBank buys:\n");
        make_auction(gi, sell);/* clients sell produce */
        init_all_request_auction(gi);
        check_all_request_fabric(gi);
        start_game(gi);
    }
}

char is_finish_game(struct global_info *gi)
{
    struct list *cl_lst = gi->cl_lst;
    struct session *sess;
    if (cl_lst)
        sess = cl_lst->sess;
    if (gi->game_now && cl_lst && !(cl_lst->next))
    {
        send_string(sess, "\nCongratulation! You win!");
        return 1;
    }
    return 0;
}

void start_server(int listener, int number_clients)
{
    struct list *tmp_lst;
    struct global_info *gl_inf = init_global_info(number_clients);
    while(!is_finish_game(gl_inf))
    {
        fd_set readfds;
        int fd, max_d = listener, res;
        inform_global_events(gl_inf);
        tmp_lst = gl_inf->cl_lst;
        FD_ZERO(&readfds);
        FD_SET(listener, &readfds);
        while(tmp_lst)
        {
            fd = (tmp_lst->sess)->fd;
            FD_SET(fd, &readfds);
            if (fd > max_d)
                max_d = fd;
            tmp_lst = tmp_lst->next;
        }
        res = select(max_d + 1, &readfds, NULL, NULL, NULL);
        if (res < 1)
        {
            perror("select");
            exit(1);
        }
        if (FD_ISSET(listener, &readfds))
            meet_client(listener, gl_inf);
        tmp_lst = gl_inf->cl_lst;
        while(tmp_lst)
        {
            struct session *sess = tmp_lst->sess;
            if (FD_ISSET(sess->fd, &readfds))
                process_data(sess, gl_inf);
            tmp_lst = tmp_lst->next;
        }
    }
}

int main(int argc, char **argv)
{
    int port, listener, number_clients;
    int pc, nc;
    if (argc != 3)
    {
        fprintf(stderr, "Bad argc \n");
        return 1;
    }
    pc = str_to_int(argv[1], &port);
    nc = str_to_int(argv[2], &number_clients);
    if (pc == -1 || nc == -1)
    {
        fprintf(stderr, "Bad argc\n");
        return 1;
    }
    listener = make_new_listen_socket(port);
    start_server(listener, number_clients);
    shutdown(listener, SHUT_RDWR);
    return 0;
}

