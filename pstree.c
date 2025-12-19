#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/procmsg.h>

// build: qcc pstree.c -o pstree -Wall -g -I ~/mainline/stage/nto/usr/include/


typedef struct Node {
    pid64_t pid;
    struct Node **children;
    int child_count;
    int printed;
} Node;

typedef struct {
    pid64_t pid;
    pid64_t ppid;
} Process;

// --- Function declarations ---
Node *find_node(Node **list, int count, pid64_t pid);
Node *create_node(pid64_t pid);
void add_child(Node *parent, Node *child);
void print_tree(Node *node, int depth);
void free_tree(Node *node);

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

/********************************/
/* get_all_pids                 */
/********************************/
pid64_t * get_all_pids(int *plen) {

    /*------------ _PROC_GETPID64_GET_ALL ---------------*/

    proc_getallpid_t msg;
    int pidlist_size = 256, rc;
    pid64_t *pidlist;

    msg.i.type = _PROC_GET_ALL_PID;
    msg.i.flags = _PROC_GET_ALL_PID_PARENT;

    pidlist = malloc(pidlist_size);

    while (pidlist) {
        rc = MsgSend_r(PROCMGR_COID, &msg.i, sizeof(msg.i), pidlist,
                       pidlist_size);

        if (rc > 0) {
            if (rc > pidlist_size) {
                free(pidlist);
                pidlist_size = rc;
                pidlist = malloc(pidlist_size);
            } else {
                *plen = rc;
                return pidlist;
            }
        } else {
            failed(MsgSend_r, -rc);
            errno = -rc;
            break;
        }
    }

    return NULL;
}


// --- Main logic ---
int main(void) {
    // Example input list: (pid, parent_pid)
    int list_len;

    Process *processes = (Process *)get_all_pids(&list_len);

    int n = list_len / sizeof(processes[0]);

    for (int i = 0; i < n; ++i) {
        printf("Process %d parent %d\n", (pid_t)processes[i].pid, (pid_t)processes[i].ppid);
    }

    Node **nodes = calloc(2 * n, sizeof(Node *));
    int node_count = 0;
    Node *root = NULL;

    // Build nodes and hierarchy
    for (int i = 0; i < n; ++i) {
        pid64_t pid = processes[i].pid;
        pid64_t ppid = processes[i].ppid;

        Node *child = find_node(nodes, node_count, pid);
        if (!child) {
            child = create_node(pid);
            printf("Create child node %d\n", (pid_t)child->pid);
            nodes[node_count++] = child;
        }

        if (ppid == 0) {
            root = child; // found root process
        } else {
            Node *parent = find_node(nodes, node_count, ppid);
            if (!parent) {
                parent = create_node(ppid);
                nodes[node_count++] = parent;
                printf("Create parent node %d\n", (pid_t)parent->pid);
            }
            printf("Add child %d to parent %d\n", (pid_t)child->pid, (pid_t)parent->pid);
            add_child(parent, child);
        }
    }

    // Print tree
    if (root) {
        print_tree(root, 0);
        for (int i = 0; i < node_count; ++i) {
            if (nodes[i]->printed)
                continue;
            print_tree(nodes[i], 0);
        }
    } else
        printf("No root process found.\n");

    // Cleanup
    for (int i = 0; i < node_count; ++i)
        free_tree(nodes[i]);
    free(nodes);

    return 0;
}

// --- Helper functions ---
Node *find_node(Node **list, int count, pid64_t pid) {
    for (int i = 0; i < count; ++i) {
        if (list[i]->pid == pid)
            return list[i];
    }
    return NULL;
}

Node *create_node(pid64_t pid) {
    Node *node = calloc(1, sizeof(Node));
    node->pid = pid;
    node->child_count = 0;
    node->children = NULL;
    return node;
}

void add_child(Node *parent, Node *child) {
    parent->children = realloc(parent->children, sizeof(Node *) * (parent->child_count + 1));
    parent->children[parent->child_count++] = child;
}

void print_tree(Node *node, int depth) {
    node->printed = 1;
    for (int i = 0; i < depth; ++i) printf("  ");
    printf("PID %d\n", (pid_t)node->pid);

    for (int i = 0; i < node->child_count; ++i)
        print_tree(node->children[i], depth + 1);
}

void free_tree(Node *node) {
    free(node->children);
    free(node);
}
