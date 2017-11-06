
struct shell_droplist_t {
    char*                       file;
    struct shell_droplist_t*    next;
};

extern struct shell_droplist_t*     shell_droplist;

void shell_droplist_entry_free(struct shell_droplist_t *ent);
struct shell_droplist_t *shell_droplist_peek(void);
struct shell_droplist_t *shell_droplist_get(void);
void shell_droplist_clear(void);

