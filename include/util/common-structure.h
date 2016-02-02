typedef uint32_t req_id_t;
typedef uint32_t view_id_t;

typedef struct view_t{
    view_id_t view_id;
    node_id_t leader_id;
    req_id_t req_id;
}view;

typedef struct view_stamp_t{
    view_id_t view_id;
    req_id_t req_id;
}view_stamp;