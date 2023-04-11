#include "sudoku.hpp"
using namespace std;

struct Node;
typedef Node Column;
struct Node
{
    Node* left;
    Node* right;
    Node* up;
    Node* down;
    Column* col;
    int name;
    int size;
};

const int kMaxNodes = 1 + 81*4 + 9*9*9*4;
const int kMaxColumns = 400;
const int kRow = 100, kCol = 200, kBox = 300;


struct Dance
{
    Column* root_;
    int*    inout_;
    Column* columns_[400];
    vector<Node*> stack_;
    Node    nodes_[kMaxNodes];
    int     cur_node_;

    Column* new_column(int n = 0)
    {
        assert(cur_node_ < kMaxNodes);
        Column* p = &nodes_[cur_node_++];
        memset(p, 0, sizeof(Column));
        p->left = p;
        p->right = p;
        p->up = p;
        p->down = p;
        p->col = p;
        p->name = n;
        return p;
    }

    void append_column(int n)
    {
        assert(columns_[n] == NULL);

        Column* p = new_column(n);
        put_left(root_, p);
        columns_[n] = p;
    }

    Node* new_row(int col)
    {
        assert(columns_[col] != NULL);
        assert(cur_node_ < kMaxNodes);

        Node* q = &nodes_[cur_node_++];

        //Node* q = new Node;
        memset(q, 0, sizeof(Node));
        q->left = q;
        q->right = q;
        q->up = q;
        q->down = q;
        q->name = col;
        q->col = columns_[col];
        put_up(q->col, q);
        return q;
    }

    int get_row_col(int row, int val)
    {
        return kRow+row*10+val;
    }

    int get_col_col(int col, int val)
    {
        return kCol+col*10+val;
    }

    int get_box_col(int box, int val)
    {
        return kBox+box*10+val;
    }

    Dance(int inout[81]) : inout_(inout), cur_node_(0)
    {
        stack_.reserve(100);

        root_ = new_column();
        root_->left = root_->right = root_;
        memset(columns_, 0, sizeof(columns_));

        bool rows[N][10] = {false};
        bool cols[N][10] = {false};
        bool boxes[N][10] = {false};

        for (int i = 0; i < N; ++i) {
            int row = i / 9;
            int col = i % 9;
            int box = row/3*3 + col/3;
            int val = inout[i];
            rows[row][val] = true;
            cols[col][val] = true;
            boxes[box][val] = true;
        }

        for (int i = 0; i < N; ++i) {
            if (inout[i] == 0) {
                append_column(i);
            }
        }

        for (int i = 0; i < 9; ++i) {
            for (int v = 1; v < 10; ++v) {
                if (!rows[i][v])
                    append_column(get_row_col(i, v));
                if (!cols[i][v])
                    append_column(get_col_col(i, v));
                if (!boxes[i][v])
                    append_column(get_box_col(i, v));
            }
        }

        for (int i = 0; i < N; ++i) {
            if (inout[i] == 0) {
                int row = i / 9;
                int col = i % 9;
                int box = row/3*3 + col/3;
                //int val = inout[i];
                for (int v = 1; v < 10; ++v) {
                    if (!(rows[row][v] || cols[col][v] || boxes[box][v])) {
                        Node* n0 = new_row(i);
                        Node* nr = new_row(get_row_col(row, v));
                        Node* nc = new_row(get_col_col(col, v));
                        Node* nb = new_row(get_box_col(box, v));
                        put_left(n0, nr);
                        put_left(n0, nc);
                        put_left(n0, nb);
                    }
                }
            }
        }
    }

    Column* get_min_column()
    {
        Column* p = root_->right;
        int min_size = p->size;
        if (min_size > 1) {
            for (Column* pp = p->right; pp != root_; pp = pp->right) {
                if (min_size > pp->size) {
                    p = pp;
                    min_size = pp->size;
                    if (min_size <= 1)
                        break;
                }
            }
        }
        return p;
    }

    void cover(Column* p)
    {
        p->right->left = p->left;
        p->left->right = p->right;
        for (Node* q = p->down; q != p; q = q->down) {
            for (Node* r = q->right; r != q; r = r->right) {
                r->down->up = r->up;
                r->up->down = r->down;
                r->col->size--;
            }
        }
    }

    void uncover(Column* p)
    {
        for (Node* r = p->up; r != p; r = r->up) {
            for (Node* q = r->left; q != r; q = q->left) {
                q->col->size++;
                q->down->up = q;
                q->up->down = q;
            }
        }
        p->right->left = p;
        p->left->right = p;
    }

    bool solve()
    {
        if (root_->left == root_) {
            for (size_t i = 0; i < stack_.size(); ++i) {
                Node* n = stack_[i];
                int cell = -1;
                int val = -1;
                while (cell == -1 || val == -1) {
                    if (n->name < 100)
                        cell = n->name;
                    else
                        val = n->name % 10;
                    n = n->right;
                }

                //assert(cell != -1 && val != -1);
                inout_[cell] = val;
            }
            return true;
        }

        Column* const col = get_min_column();
        cover(col);
        for (Node* r = col->down; r != col; r = r->down) {
            stack_.push_back(r);
            for (Node* q = r->right; q != r; q = q->right) {
                cover(q->col);
            }
            if (solve()) {
                return true;
            }
            stack_.pop_back();
            for (Node* q = r->left; q != r; q = q->left) {
                uncover(q->col);
            }
        }
        uncover(col);
        return false;
    }

    void put_left(Column* old, Column* nnew)
    {
        nnew->left = old->left;
        nnew->right = old;
        old->left->right = nnew;
        old->left = nnew;
    }

    void put_up(Column* old, Node* nnew)
    {
        nnew->up = old->up;
        nnew->down = old;
        old->up->down = nnew;
        old->up = nnew;
        old->size++;
        nnew->col = old;
    }
    void get_answer(char * answer,int board[81])
    {
        for (int i = 0; i < N; i++)
        {
            answer[i] = board[i] + '0';
        }
    }
};



void input(char *problem,int board[81])
{
  for (int cell = 0; cell < N; ++cell) {
    board[cell] = problem[cell] - '0';
    // assert(0 <= board[cell] && board[cell] <= NUM);
    if(!(0 <= board[cell] && board[cell] <= NUM))
    {
        printf("%d\n",board[cell]);
        assert(0 <= board[cell] && board[cell] <= NUM);
    }
  }
  // find_spaces();
}

void solver(char *problem,int board[81])
{
  input(problem,board);
  Dance d(board);
  int rc=d.solve();
  d.get_answer(problem,board);
}
