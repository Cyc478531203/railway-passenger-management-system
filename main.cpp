#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <windows.h>
#include <conio.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

using namespace std;

namespace Ui {
constexpr const char* reset = "\x1b[0m";
constexpr const char* bold = "\x1b[1m";
constexpr const char* cyan = "\x1b[36m";
constexpr const char* green = "\x1b[32m";
constexpr const char* yellow = "\x1b[33m";
constexpr const char* red = "\x1b[31m";
constexpr const char* blue = "\x1b[34m";
constexpr const char* magenta = "\x1b[35m";

void enableAnsiColors() {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(output, &mode)) {
        SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}

void clearScreen() {
    cout << "\x1b[2J\x1b[H";
}
}

enum class SeatClass { First = 1, Second = 2 };
enum class PassengerStatus { Booked, OnBoard };

string className(SeatClass type) {
    return type == SeatClass::First ? "一等座" : "二等座";
}

const char* classColor(SeatClass type) {
    return type == SeatClass::First ? Ui::magenta : Ui::blue;
}

bool validIdLast4(const string& idLast4) {
    if (idLast4.size() != 4) return false;
    for (int i = 0; i < 3; ++i) {
        if (!isdigit(static_cast<unsigned char>(idLast4[i]))) return false;
    }
    return isdigit(static_cast<unsigned char>(idLast4[3])) || idLast4[3] == 'X';
}

struct Passenger {
    long long ticketNo{};
    string idLast4;
    string name;
    int from{};
    int to{};
    int fare{};
    int carriageNo{};
    int seatNo{};
    SeatClass seatClass{SeatClass::Second};
    PassengerStatus status{PassengerStatus::Booked};
};

// 跳表：每个车厢中的旅客按票号排序保存，所有结点均通过多级指针相连。
class PassengerSkipList {
    static constexpr int MAX_LEVEL = 6;

    struct Node {
        long long key;
        Passenger* passenger;
        vector<Node*> next;
        Node(long long k, Passenger* p, int level)
            : key(k), passenger(p), next(level + 1, nullptr) {}
    };

    Node* head;
    int currentLevel = 0;

    mt19937 rng{random_device{}()};

    int randomLevel() {
        int level = 0;
        while (level < MAX_LEVEL - 1 && uniform_int_distribution<int>(0, 1)(rng)) {
            ++level;
        }
        return level;
    }

public:
    PassengerSkipList() : head(new Node(numeric_limits<long long>::min(), nullptr, MAX_LEVEL - 1)) {}
    PassengerSkipList(const PassengerSkipList&) = delete;
    PassengerSkipList& operator=(const PassengerSkipList&) = delete;

    ~PassengerSkipList() {
        Node* cur = head->next[0];
        while (cur) {
            Node* next = cur->next[0];
            delete cur->passenger;
            delete cur;
            cur = next;
        }
        delete head;
    }

    void insert(Passenger* passenger) {
        Node* update[MAX_LEVEL]{};
        Node* cur = head;
        for (int level = currentLevel; level >= 0; --level) {
            while (cur->next[level] && cur->next[level]->key < passenger->ticketNo) {
                cur = cur->next[level];
            }
            update[level] = cur;
        }

        int level = randomLevel();
        if (level > currentLevel) {
            for (int i = currentLevel + 1; i <= level; ++i) update[i] = head;
            currentLevel = level;
        }
        Node* node = new Node(passenger->ticketNo, passenger, level);
        for (int i = 0; i <= level; ++i) {
            node->next[i] = update[i]->next[i];
            update[i]->next[i] = node;
        }
    }

    Passenger* erase(long long ticketNo) {
        Node* update[MAX_LEVEL]{};
        Node* cur = head;
        for (int level = currentLevel; level >= 0; --level) {
            while (cur->next[level] && cur->next[level]->key < ticketNo) {
                cur = cur->next[level];
            }
            update[level] = cur;
        }
        cur = cur->next[0];
        if (!cur || cur->key != ticketNo) return nullptr;

        for (int i = 0; i <= currentLevel; ++i) {
            if (update[i]->next[i] != cur) break;
            update[i]->next[i] = cur->next[i];
        }
        while (currentLevel > 0 && head->next[currentLevel] == nullptr) --currentLevel;
        Passenger* passenger = cur->passenger;
        delete cur;
        return passenger;
    }

    template <typename Function>
    void forEach(Function fn) const {
        Node* cur = head->next[0];
        while (cur) {
            fn(cur->passenger);
            cur = cur->next[0];
        }
    }
};

// 链地址哈希表：按身份证后四位快速定位当前仍在列车记录中的旅客。
class PassengerHashIndex {
    struct HashNode {
        Passenger* passenger;
        HashNode* next;
    };

    vector<HashNode*> buckets;

    size_t bucketOf(const string& key) const {
        unsigned long hash = 5381;
        for (unsigned char ch : key) hash = ((hash << 5) + hash) + ch;
        return hash % buckets.size();
    }

public:
    explicit PassengerHashIndex(size_t bucketCount = 53) : buckets(bucketCount, nullptr) {}
    PassengerHashIndex(const PassengerHashIndex&) = delete;
    PassengerHashIndex& operator=(const PassengerHashIndex&) = delete;

    ~PassengerHashIndex() {
        for (HashNode* head : buckets) {
            while (head) {
                HashNode* next = head->next;
                delete head;
                head = next;
            }
        }
    }

    void insert(Passenger* passenger) {
        size_t bucket = bucketOf(passenger->idLast4);
        buckets[bucket] = new HashNode{passenger, buckets[bucket]};
    }

    void remove(Passenger* passenger) {
        size_t bucket = bucketOf(passenger->idLast4);
        HashNode** link = &buckets[bucket];
        while (*link) {
            if ((*link)->passenger == passenger) {
                HashNode* old = *link;
                *link = old->next;
                delete old;
                return;
            }
            link = &((*link)->next);
        }
    }

    vector<Passenger*> find(const string& idLast4) const {
        vector<Passenger*> result;
        for (HashNode* node = buckets[bucketOf(idLast4)]; node; node = node->next) {
            if (node->passenger->idLast4 == idLast4) result.push_back(node->passenger);
        }
        return result;
    }
};

// 有效售票流水使用链表保存；下车记录保留，退票记录从流水中撤销。
class SalesLedger {
    struct SaleNode {
        Passenger record;
        SaleNode* next;
    };

    SaleNode* head{nullptr};
    SaleNode* tail{nullptr};

public:
    SalesLedger() = default;
    SalesLedger(const SalesLedger&) = delete;
    SalesLedger& operator=(const SalesLedger&) = delete;

    ~SalesLedger() {
        while (head) {
            SaleNode* next = head->next;
            delete head;
            head = next;
        }
    }

    void add(const Passenger& passenger) {
        SaleNode* node = new SaleNode{passenger, nullptr};
        if (!head) head = tail = node;
        else {
            tail->next = node;
            tail = node;
        }
    }

    const Passenger* find(long long ticketNo) const {
        for (SaleNode* cur = head; cur; cur = cur->next) {
            if (cur->record.ticketNo == ticketNo) return &cur->record;
        }
        return nullptr;
    }

    bool remove(long long ticketNo) {
        SaleNode** link = &head;
        SaleNode* previous = nullptr;
        while (*link) {
            if ((*link)->record.ticketNo == ticketNo) {
                SaleNode* old = *link;
                *link = old->next;
                if (tail == old) tail = previous;
                delete old;
                return true;
            }
            previous = *link;
            link = &((*link)->next);
        }
        return false;
    }

    template <typename Function>
    void forEach(Function fn) const {
        for (SaleNode* cur = head; cur; cur = cur->next) fn(cur->record);
    }

    bool hasPassenger(const string& idLast4) const {
        for (SaleNode* cur = head; cur; cur = cur->next) {
            if (cur->record.idLast4 == idLast4) return true;
        }
        return false;
    }
};

// AVL票价索引：树节点按票价排序，同票价乘客使用链表连接。
// 只引用乘客，不负责删除Passenger对象。
class PassengerFareTree {
    struct PassengerLink {
        Passenger* passenger;
        PassengerLink* next;
    };
    struct FareNode {
        int fare;
        int height;
        PassengerLink* passengers;
        FareNode* left;
        FareNode* right;
    };
    FareNode* root{nullptr};

    static int height(FareNode* node) { return node ? node->height : 0; }
    static void update(FareNode* node) {
        node->height = 1 + max(height(node->left), height(node->right));
    }
    static FareNode* rotateRight(FareNode* node) {
        FareNode* top = node->left;
        node->left = top->right;
        top->right = node;
        update(node);
        update(top);
        return top;
    }
    static FareNode* rotateLeft(FareNode* node) {
        FareNode* top = node->right;
        node->right = top->left;
        top->left = node;
        update(node);
        update(top);
        return top;
    }
    static FareNode* balance(FareNode* node) {
        if (!node) return nullptr;
        update(node);
        int difference = height(node->left) - height(node->right);
        if (difference > 1) {
            if (height(node->left->left) < height(node->left->right))
                node->left = rotateLeft(node->left);
            return rotateRight(node);
        }
        if (difference < -1) {
            if (height(node->right->right) < height(node->right->left))
                node->right = rotateRight(node->right);
            return rotateLeft(node);
        }
        return node;
    }
    static FareNode* insertNode(FareNode* node, Passenger* passenger) {
        if (!node) return new FareNode{passenger->fare, 1,
            new PassengerLink{passenger, nullptr}, nullptr, nullptr};
        if (passenger->fare < node->fare) node->left = insertNode(node->left, passenger);
        else if (passenger->fare > node->fare) node->right = insertNode(node->right, passenger);
        else {
            node->passengers = new PassengerLink{passenger, node->passengers};
            return node;
        }
        return balance(node);
    }
    // 删除链表已空的票价节点；双孩子节点使用中序后继替换。
    static FareNode* eraseEmptyNode(FareNode* node, int fare) {
        if (!node) return nullptr;
        if (fare < node->fare) node->left = eraseEmptyNode(node->left, fare);
        else if (fare > node->fare) node->right = eraseEmptyNode(node->right, fare);
        else {
            if (!node->left || !node->right) {
                FareNode* child = node->left ? node->left : node->right;
                delete node;
                return child;
            }
            FareNode* successor = node->right;
            while (successor->left) successor = successor->left;
            node->fare = successor->fare;
            node->passengers = successor->passengers;
            successor->passengers = nullptr;
            node->right = eraseEmptyNode(node->right, successor->fare);
        }
        return balance(node);
    }
    static FareNode* removeNode(FareNode* node, Passenger* passenger) {
        if (!node) return nullptr;
        if (passenger->fare < node->fare) node->left = removeNode(node->left, passenger);
        else if (passenger->fare > node->fare) node->right = removeNode(node->right, passenger);
        else {
            PassengerLink** link = &node->passengers;
            while (*link && (*link)->passenger != passenger) link = &((*link)->next);
            if (*link) {
                PassengerLink* old = *link;
                *link = old->next;
                delete old;
            }
            if (!node->passengers) return eraseEmptyNode(node, node->fare);
        }
        return balance(node);
    }
    static void destroy(FareNode* node) {
        if (!node) return;
        destroy(node->left);
        destroy(node->right);
        while (node->passengers) {
            PassengerLink* next = node->passengers->next;
            delete node->passengers;
            node->passengers = next;
        }
        delete node;
    }
public:
    PassengerFareTree() = default;
    PassengerFareTree(const PassengerFareTree&) = delete;
    PassengerFareTree& operator=(const PassengerFareTree&) = delete;
    ~PassengerFareTree() { destroy(root); }
    void insert(Passenger* passenger) { root = insertNode(root, passenger); }
    void remove(Passenger* passenger) { root = removeNode(root, passenger); }
    vector<Passenger*> find(int fare) const {
        FareNode* node = root;
        while (node && node->fare != fare)
            node = fare < node->fare ? node->left : node->right;
        vector<Passenger*> result;
        if (node) {
            for (PassengerLink* link = node->passengers; link; link = link->next)
                result.push_back(link->passenger);
        }
        return result;
    }
};

struct Carriage {
    int number;
    SeatClass type;
    int seatCount;
    PassengerSkipList passengers;
    PassengerFareTree fareIndex; // 当前车厢按票价建立的辅助平衡树索引

    Carriage(int n, SeatClass t, int count) : number(n), type(t), seatCount(count) {}
    Carriage(const Carriage&) = delete;
    Carriage& operator=(const Carriage&) = delete;
};


class TrainSystem {
    const string trainNo = "G1001";
    const vector<string> stations{"上海", "苏州", "南京", "济南", "天津", "北京"};
    vector<Carriage*> carriages;
    PassengerHashIndex index;
    SalesLedger ledger;
    long long nextTicketNo{100001};

    static bool overlaps(int fromA, int toA, int fromB, int toB) {
        return max(fromA, fromB) < min(toA, toB);
    }

    int readInt(const string& prompt, int low, int high) const {
        while (true) {
            cout << prompt;
            string line;
            if (!getline(cin, line)) return low;
            stringstream ss(line);
            int value;
            char extra;
            if ((ss >> value) && !(ss >> extra) && value >= low && value <= high) return value;
            cout << "输入无效，请输入 " << low << "～" << high << " 之间的整数。\n";
        }
    }

    long long readTicketNo(const string& prompt) const {
        while (true) {
            cout << prompt;
            string line;
            if (!getline(cin, line)) return -1;
            stringstream ss(line);
            long long value;
            char extra;
            if ((ss >> value) && !(ss >> extra) && value > 0) return value;
            cout << Ui::red << "输入无效，请输入正确的数字票号。\n" << Ui::reset;
        }
    }

    int chooseStation(const string& prompt) const {
        cout << "站点：";
        for (size_t i = 0; i < stations.size(); ++i) {
            cout << i + 1 << "." << stations[i];
            if (i + 1 != stations.size()) cout << "  ";
        }
        cout << '\n';
        return readInt(prompt, 1, static_cast<int>(stations.size())) - 1;
    }

    Carriage* getCarriage(int number) const {
        if (number < 1 || number > static_cast<int>(carriages.size())) return nullptr;
        return carriages[number - 1];
    }

    bool seatAvailable(const Carriage& carriage, int seat, int from, int to) const {
        bool available = true;
        carriage.passengers.forEach([&](Passenger* p) {
            if (p->seatNo == seat && overlaps(p->from, p->to, from, to)) available = false;
        });
        return available;
    }

    int remainingCount(int from, int to, int classChoice = 0) const {
        int remaining = 0;
        for (const Carriage* carriage : carriages) {
            if (classChoice != 0 && static_cast<int>(carriage->type) != classChoice) continue;
            for (int seat = 1; seat <= carriage->seatCount; ++seat) {
                if (seatAvailable(*carriage, seat, from, to)) ++remaining;
            }
        }
        return remaining;
    }

    void showAvailableSeats(int from, int to, SeatClass type) const {
        cout << classColor(type) << Ui::bold << className(type) << Ui::reset
             << "可售车厢和座位：\n";
        for (const Carriage* carriage : carriages) {
            if (carriage->type != type) continue;
            cout << classColor(type) << Ui::bold << "  " << carriage->number << "车厢："
                 << Ui::reset;
            bool any = false;
            for (int seat = 1; seat <= carriage->seatCount; ++seat) {
                if (seatAvailable(*carriage, seat, from, to)) {
                    cout << setw(2) << setfill('0') << seat << setfill(' ') << "号 ";
                    any = true;
                }
            }
            if (!any) cout << Ui::red << "已满" << Ui::reset;
            cout << '\n';
        }
    }

    pair<Carriage*, int> chooseSpecificSeat(int from, int to, SeatClass type) const {
        while (true) {
            showAvailableSeats(from, to, type);
            int carriageNo = readInt("请选择车厢号：", 1, static_cast<int>(carriages.size()));
            Carriage* carriage = getCarriage(carriageNo);
            if (carriage->type != type) {
                cout << Ui::red << "选择失败：该车厢不是" << className(type) << "车厢。\n"
                     << Ui::reset;
                continue;
            }
            bool carriageHasSeat = false;
            for (int seat = 1; seat <= carriage->seatCount; ++seat) {
                if (seatAvailable(*carriage, seat, from, to)) {
                    carriageHasSeat = true;
                    break;
                }
            }
            if (!carriageHasSeat) {
                cout << Ui::red << "选择失败：该车厢在所选区间已经没有空座，请选择其他车厢。\n"
                     << Ui::reset;
                continue;
            }

            int seat = readInt("请选择座位号：", 1, carriage->seatCount);
            if (!seatAvailable(*carriage, seat, from, to)) {
                cout << Ui::red << "选择失败：该座位在所选区间已被占用，请重新选择。\n"
                     << Ui::reset;
                continue;
            }
            return {carriage, seat};
        }
    }

    void printPassenger(const Passenger& p) const {
        cout << "票号 " << p.ticketNo << "，" << p.name << "（证件后四位 " << p.idLast4 << "），"
             << stations[p.from] << "→" << stations[p.to] << "，"
             << p.carriageNo << "车厢 " << setw(2) << setfill('0') << p.seatNo << setfill(' ')
             << "号，" << classColor(p.seatClass) << Ui::bold << className(p.seatClass)
             << Ui::reset << "，票价 " << p.fare << " 元";
    }

    int chooseSeatClass(bool includeAll) const {
        cout << "席别（";
        if (includeAll) cout << "0=全部，";
        cout << Ui::magenta << Ui::bold << "1=一等座" << Ui::reset << "，"
             << Ui::blue << Ui::bold << "2=二等座" << Ui::reset << "）：\n";
        return readInt("请选择席别：", includeAll ? 0 : 1, 2);
    }

    void recommendOtherTrains(int from, int to, SeatClass type) const {
        cout << classColor(type) << Ui::bold << className(type) << Ui::reset
             << "在 " << stations[from] << "→" << stations[to]
             << " 区间已售完。可尝试其他车次：G1003、G1005、G1007（演示数据，请以实际余票为准）。\n";
    }

public:
    TrainSystem() {
        carriages.push_back(new Carriage(1, SeatClass::First, 5));
        carriages.push_back(new Carriage(2, SeatClass::First, 5));
        carriages.push_back(new Carriage(3, SeatClass::Second, 9));
        carriages.push_back(new Carriage(4, SeatClass::Second, 9));
        carriages.push_back(new Carriage(5, SeatClass::Second, 9));
    }

    ~TrainSystem() {
        for (Carriage* carriage : carriages) delete carriage;
    }

    void showTrainInfo() const {
        cout << Ui::cyan << Ui::bold << "\n========== 列车基本信息 ==========\n" << Ui::reset
             << "车次：" << trainNo << '\n'
             << "运行区间：上海→苏州→南京→济南→天津→北京\n"
             << "车厢总数：5 节；座位总量：37 个\n"
             << Ui::magenta << Ui::bold << "一等座：1～2 号车厢，每节 5 座，共 10 座\n" << Ui::reset
             << Ui::blue << Ui::bold << "二等座：3～5 号车厢，每节 9 座，共 27 座\n" << Ui::reset
             << "票价：每经过一个区间 150 元，" << Ui::magenta << Ui::bold
             << "一等座" << Ui::reset << "整张票另加 100 元\n"
             << "全程当前余票：" << remainingCount(0, 5) << " 张（"
             << Ui::magenta << Ui::bold << "一等座 " << remainingCount(0, 5, 1) << Ui::reset
             << "，" << Ui::blue << Ui::bold << "二等座 " << remainingCount(0, 5, 2)
             << Ui::reset << "）\n";

        cout << "全部15种乘车区间余票：\n";
        for (int from = 0; from < static_cast<int>(stations.size()) - 1; ++from) {
            for (int to = from + 1; to < static_cast<int>(stations.size()); ++to) {
                cout << "  " << stations[from] << "→" << stations[to] << "："
                     << remainingCount(from, to) << " 张（"
                     << Ui::magenta << Ui::bold << "一等座 " << remainingCount(from, to, 1)
                     << Ui::reset << "，" << Ui::blue << Ui::bold << "二等座 "
                     << remainingCount(from, to, 2) << Ui::reset << "）\n";
            }
        }
    }

    void queryTickets() const {
        cout << Ui::cyan << Ui::bold << "\n========== 查询可售车票 ==========\n" << Ui::reset;
        int from = chooseStation("请选择上车站：");
        int to = chooseStation("请选择下车站：");
        if (from >= to) {
            cout << "查询失败：下车站必须位于上车站之后。\n";
            return;
        }
        int choice = chooseSeatClass(true);
        int count = 0;
        cout << stations[from] << "→" << stations[to] << " 可售座位：\n";
        for (const Carriage* carriage : carriages) {
            if (choice && static_cast<int>(carriage->type) != choice) continue;
            cout << classColor(carriage->type) << Ui::bold << "  " << carriage->number
                 << "车厢（" << className(carriage->type) << "）：" << Ui::reset;
            bool any = false;
            for (int seat = 1; seat <= carriage->seatCount; ++seat) {
                if (seatAvailable(*carriage, seat, from, to)) {
                    cout << setw(2) << setfill('0') << seat << setfill(' ') << "号 ";
                    ++count;
                    any = true;
                }
            }
            if (!any) cout << "无";
            cout << '\n';
        }
        if (count == 0) cout << "查询无结果：指定区间和席别没有可售座位。\n";
        else cout << "合计可售 " << count << " 张。\n";
    }

    void sellTicket() {
        cout << Ui::cyan << Ui::bold << "\n========== 售票 ==========\n" << Ui::reset;
        int from = chooseStation("请选择上车站：");
        int to = chooseStation("请选择下车站：");
        if (from >= to) {
            cout << "售票失败：下车站必须位于上车站之后。\n";
            return;
        }
        int classChoice = chooseSeatClass(false);
        SeatClass type = static_cast<SeatClass>(classChoice);
        if (remainingCount(from, to, classChoice) == 0) {
            cout << classColor(type) << Ui::bold << className(type) << Ui::reset
                 << "在 " << stations[from] << "→" << stations[to] << " 区间已售完。\n";

            if (type == SeatClass::First && remainingCount(from, to, 2) > 0) {
                cout << Ui::blue << Ui::bold << "本车次二等座还有 "
                     << remainingCount(from, to, 2) << " 张。" << Ui::reset << '\n';
                int change = readInt("是否改购二等座（1=是，0=否）：", 0, 1);
                if (change == 1) {
                    type = SeatClass::Second;
                    classChoice = 2;
                } else {
                    recommendOtherTrains(from, to, SeatClass::First);
                    return;
                }
            } else {
                recommendOtherTrains(from, to, type);
                return;
            }
        }

        auto [carriage, seat] = chooseSpecificSeat(from, to, type);
        if (!carriage) {
            recommendOtherTrains(from, to, type);
            return;
        }

        string name;
        cout << "旅客姓名：";
        getline(cin, name);
        if (name.empty()) {
            cout << "售票失败：旅客姓名不能为空。\n";
            return;
        }
        string last4;
        cout << "身份证后四位字符：";
        getline(cin, last4);
        if (!validIdLast4(last4)) {
            cout << Ui::red << "售票失败：身份证后四位的前三位必须是数字，最后一位必须是数字或大写X。\n"
                 << Ui::reset;
            return;
        }

        int fare = (to - from) * 150 + (type == SeatClass::First ? 100 : 0);
        Passenger* passenger = new Passenger{nextTicketNo++, last4, name, from, to, fare,
                                             carriage->number, seat, type, PassengerStatus::Booked};
        carriage->passengers.insert(passenger);
        carriage->fareIndex.insert(passenger);
        index.insert(passenger);
        ledger.add(*passenger);

        cout << Ui::green << Ui::bold << "售票成功：" << Ui::reset;
        printPassenger(*passenger);
        cout << "。\n";
        cout << "当前该区间剩余：共 " << remainingCount(from, to) << " 张；"
             << classColor(type) << Ui::bold << className(type) << Ui::reset << " "
             << remainingCount(from, to, classChoice) << " 张。\n";
    }

    void refundTicket() {
        cout << Ui::cyan << Ui::bold << "\n========== 办理退票 ==========\n" << Ui::reset;
        long long ticketNo = readTicketNo("请输入需要退票的票号：");

        Passenger* passenger = nullptr;
        Carriage* owner = nullptr;
        for (Carriage* carriage : carriages) {
            carriage->passengers.forEach([&](Passenger* current) {
                if (!passenger && current->ticketNo == ticketNo) {
                    passenger = current;
                    owner = carriage;
                }
            });
        }

        if (!passenger) {
            if (ledger.find(ticketNo)) {
                cout << Ui::red << "退票失败：该票对应旅客已经下车，不能办理退票。\n" << Ui::reset;
            } else {
                cout << Ui::red << "退票失败：没有找到该有效票号，车票可能已退或票号输入错误。\n"
                     << Ui::reset;
            }
            return;
        }

        if (passenger->status == PassengerStatus::OnBoard) {
            cout << Ui::red << "退票失败：旅客已经办理上车，不能再办理退票。\n" << Ui::reset;
            return;
        }

        cout << "待退车票：";
        printPassenger(*passenger);
        cout << '\n';
        int confirm = readInt("确认退票（1=确认，0=取消）：", 0, 1);
        if (confirm == 0) {
            cout << Ui::yellow << "已取消退票。\n" << Ui::reset;
            return;
        }

        int from = passenger->from;
        int to = passenger->to;
        int fare = passenger->fare;
        SeatClass type = passenger->seatClass;
        int classChoice = static_cast<int>(type);

        index.remove(passenger);
        owner->fareIndex.remove(passenger);
        Passenger* removed = owner->passengers.erase(ticketNo);
        bool ledgerRemoved = ledger.remove(ticketNo);
        delete removed;

        if (!ledgerRemoved) {
            cout << Ui::red << "退票异常：售票流水未找到该票号，请检查数据。\n" << Ui::reset;
            return;
        }

        cout << Ui::green << Ui::bold << "退票成功：" << Ui::reset
             << "票号 " << ticketNo << "，退还票款 " << fare << " 元。\n"
             << "当前该区间余票：共 " << remainingCount(from, to) << " 张；"
             << classColor(type) << Ui::bold << className(type) << Ui::reset << " "
             << remainingCount(from, to, classChoice) << " 张。\n";
    }

    void searchPassenger() const {
        cout << Ui::cyan << Ui::bold << "\n========== 查询旅客 ==========\n" << Ui::reset
             << "身份证后四位字符：";
        string last4;
        getline(cin, last4);
        if (!validIdLast4(last4)) {
            cout << Ui::red << "查询失败：身份证后四位的前三位必须是数字，最后一位必须是数字或大写X。\n"
                 << Ui::reset;
            return;
        }
        vector<Passenger*> found = index.find(last4);
        if (found.empty()) {
            if (ledger.hasPassenger(last4)) {
                cout << "查询失败：对应旅客已经下车，其车厢旅客记录已删除；历史售票流水仍保留。\n";
            } else {
                cout << "查询失败：售票流水中没有该证件后四位，请检查输入或确认旅客是否已购票。\n";
            }
            return;
        }
        if (found.size() > 1) cout << "证件后四位可能重复，共找到 " << found.size() << " 条记录：\n";
        for (Passenger* p : found) {
            cout << "  ";
            printPassenger(*p);
            cout << "，状态：" << (p->status == PassengerStatus::Booked ? "待上车" : "已上车") << '\n';
        }
    }

    void processBoarding() {
        cout << Ui::cyan << Ui::bold << "\n========== 办理上车 ==========\n" << Ui::reset;
        int station = chooseStation("请选择当前车站：");
        if (station == static_cast<int>(stations.size()) - 1) {
            cout << "办理失败：北京是终点站，没有本站始发的旅客。\n";
            return;
        }
        int total = 0;
        for (Carriage* carriage : carriages) {
            int carriageCount = 0;
            carriage->passengers.forEach([&](Passenger* p) {
                if (p->from == station && p->status == PassengerStatus::Booked) {
                    p->status = PassengerStatus::OnBoard;
                    if (carriageCount == 0) cout << carriage->number << "车厢上车旅客：\n";
                    cout << "  ";
                    printPassenger(*p);
                    cout << '\n';
                    ++carriageCount;
                    ++total;
                }
            });
        }
        if (total == 0) cout << "没有需要在 " << stations[station] << " 办理上车的旅客。\n";
        else cout << "本站共办理上车 " << total << " 人。\n";
    }

    void processAlighting() {
        cout << Ui::cyan << Ui::bold << "\n========== 办理下车并删除车厢记录 ==========\n" << Ui::reset;
        int station = chooseStation("请选择当前车站：");
        if (station == 0) {
            cout << "办理失败：上海是起点站，没有本站到达的旅客。\n";
            return;
        }
        int total = 0;
        for (Carriage* carriage : carriages) {
            vector<Passenger*> leaving;
            carriage->passengers.forEach([&](Passenger* p) {
                if (p->to == station) leaving.push_back(p);
            });
            if (!leaving.empty()) cout << carriage->number << "车厢下车旅客：\n";
            for (Passenger* p : leaving) {
                cout << "  ";
                printPassenger(*p);
                if (p->status == PassengerStatus::Booked) cout << "（提示：该旅客未办理上车）";
                cout << '\n';
                index.remove(p);
                carriage->fareIndex.remove(p);
                Passenger* removed = carriage->passengers.erase(p->ticketNo);
                delete removed;
                ++total;
            }
        }
        if (total == 0) cout << "没有需要在 " << stations[station] << " 下车的旅客。\n";
        else cout << "本站共删除 " << total << " 条已下车旅客记录；售票流水仍保留用于统计。\n";
    }

    void stationStatistics() const {
        cout << Ui::cyan << Ui::bold << "\n========== 各站客流与收入统计 ==========\n" << Ui::reset;
        int board[6]{};
        int alight[6]{};
        int boardByCarriage[6][5]{};
        long long income = 0;
        int ticketCount = 0;
        int ticketsByClass[2]{};
        long long incomeByClass[2]{};
        ledger.forEach([&](const Passenger& p) {
            ++board[p.from];
            ++alight[p.to];
            ++boardByCarriage[p.from][p.carriageNo - 1];
            income += p.fare;
            ++ticketCount;
            int classIndex = p.seatClass == SeatClass::First ? 0 : 1;
            ++ticketsByClass[classIndex];
            incomeByClass[classIndex] += p.fare;
        });

        for (int i = 0; i < 6; ++i) {
            cout << stations[i] << "：下车 " << alight[i] << " 人，即将上车 " << board[i]
                 << " 人；各车厢上车分布：";
            for (int c = 0; c < 5; ++c) cout << c + 1 << "车:" << boardByCarriage[i][c] << " ";
            cout << '\n';
        }
        cout << "当前有效售票：" << ticketCount << " 张；扣除退票后的所得款项：" << income << " 元。\n";
        cout << Ui::magenta << Ui::bold << "一等座" << Ui::reset << "：售出 "
             << ticketsByClass[0] << " 张，收入 " << incomeByClass[0] << " 元；"
             << Ui::blue << Ui::bold << "二等座" << Ui::reset << "：售出 "
             << ticketsByClass[1] << " 张，收入 " << incomeByClass[1] << " 元。\n";
        cout << "系统显示所选区间的全部可售座位，由购票人自主选择车厢和座位号。\n";
    }

    void searchByFare() const {
        cout << Ui::cyan << Ui::bold << "\n========== 按票价查询旅客（AVL树） ==========\n" << Ui::reset;
        int fare = readInt("请输入票价（元）：", 0, numeric_limits<int>::max());
        int count = 0;
        for (const Carriage* carriage : carriages) {
            vector<Passenger*> found = carriage->fareIndex.find(fare);
            if (found.empty()) continue;
            cout << classColor(carriage->type) << Ui::bold << carriage->number << "车厢（"
                 << className(carriage->type) << "）：\n" << Ui::reset;
            for (Passenger* passenger : found) {
                printPassenger(*passenger);
                cout << "，状态：" << (passenger->status == PassengerStatus::Booked ? "待上车" : "已上车") << '\n';
                ++count;
            }
        }
        if (!count) cout << Ui::red << "查询无结果：当前车厢记录中没有该票价的旅客（可能已退票或下车）。\n" << Ui::reset;
        else cout << "共找到 " << count << " 位旅客。\n";
    }

    int visualMenu() const {
        const vector<string> items{
            "显示列车信息",
            "查询所有可售车票（车厢及座位号）",
            "完成售票",
            "办理退票",
            "查询旅客信息",
            "添加各车厢上车旅客信息",
            "删除各车厢下车旅客信息",
            "分类统计各站上下车人数和收入",
            "按票价查询旅客（AVL平衡树）",
            "退出系统"
        };
        int selected = 0;

        while (true) {
            Ui::clearScreen();
            cout << Ui::cyan << Ui::bold
                 << "+------------------------------------------------------------+\n"
                 << "|              G1001 高铁列车旅客管理信息系统               |\n"
                 << "+------------------------------------------------------------+\n"
                 << Ui::reset
                 << "  上海 -> 苏州 -> 南京 -> 济南 -> 天津 -> 北京\n"
                 << "  车厢 5 节   座位 37 个   全程当前余票 "
                 << Ui::green << remainingCount(0, 5) << Ui::reset << " 张\n"
                 << "+------------------------------------------------------------+\n";

            for (int i = 0; i < static_cast<int>(items.size()); ++i) {
                if (i == selected) {
                    cout << Ui::yellow << Ui::bold << "  > " << items[i] << Ui::reset << '\n';
                } else {
                    cout << "    " << items[i] << '\n';
                }
            }
            cout << "+------------------------------------------------------------+\n"
                 << "  使用方向键 上/下 选择，按 Enter 确认。" << flush;

            int key = _getch();
            if (key == 0 || key == 224) {
                key = _getch();
                if (key == 72) selected = (selected - 1 + static_cast<int>(items.size())) % static_cast<int>(items.size());
                if (key == 80) selected = (selected + 1) % static_cast<int>(items.size());
            } else if (key == 13) {
                return selected == static_cast<int>(items.size()) - 1 ? 0 : selected + 1;
            }
        }
    }

    void pauseVisualPage() const {
        cout << Ui::cyan << "\n按任意键返回主菜单……" << Ui::reset << flush;
        _getch();
    }

    void run() {
        while (true) {
            int choice = visualMenu();
            Ui::clearScreen();
            switch (choice) {
                case 1: showTrainInfo(); break;
                case 2: queryTickets(); break;
                case 3: sellTicket(); break;
                case 4: refundTicket(); break;
                case 5: searchPassenger(); break;
                case 6: processBoarding(); break;
                case 7: processAlighting(); break;
                case 8: stationStatistics(); break;
                case 9: searchByFare(); break;
                case 0: cout << "系统已退出。\n"; return;
            }
            pauseVisualPage();
        }
    }
};

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    Ui::enableAnsiColors();

    TrainSystem system;
    system.run();
    return 0;
}
