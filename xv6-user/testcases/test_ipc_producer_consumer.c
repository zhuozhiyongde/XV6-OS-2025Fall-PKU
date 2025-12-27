#include "test.h"

#define BUFFER_SIZE 5
#define PRODUCER_COUNT 2
#define CONSUMER_COUNT 2
#define ITEMS_PER_PRODUCER 4

int pipe_fd[2];  // 主管道用于生产者消费者通信
int producer_log_fd[2];  // 管道用于记录生产的数据
int consumer_log_fd[2];  // 管道用于记录消费的数据
int empty_sem, full_sem, mutex_sem;

// 简单的整数转字符串函数
void itoa(int n, char *str) {
    int i = 0;
    int sign = n;

    if (n < 0) n = -n;

    // 处理个位数
    if (n == 0) {
        str[i++] = '0';
    } else {
        // 反转数字
        char temp[16];
        int j = 0;
        while (n > 0) {
            temp[j++] = '0' + (n % 10);
            n /= 10;
        }
        // 反转回来
        while (j > 0) {
            str[i++] = temp[--j];
        }
    }

    if (sign < 0) {
        str[i++] = '-';
    }
    str[i] = '\0';
}

// 记录数据到日志管道
void log_data(int log_fd, int data) {
    char buf[16];
    itoa(data, buf);
    write(log_fd, buf, 16);
}

void producer(int id) {
    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        int item = id * 100 + i;

        sem_p(empty_sem);  // 等待空位
        sem_p(mutex_sem);  // 进入临界区

        // 生产物品（写入管道）
        char buf[16];
        itoa(item, buf);
        write(pipe_fd[1], buf, 16);
        
        // 记录生产的数据到日志管道
        log_data(producer_log_fd[1], item);
        printf("Prod %d produced %d\n", id, item);

        sem_v(mutex_sem);  // 离开临界区
        sem_v(full_sem);   // 增加满位

        sleep(1);  // 模拟生产时间
    }
    printf("Prod %d finished\n", id);
}

void consumer(int id) {
    int count = 0;
    int total_items = (PRODUCER_COUNT * ITEMS_PER_PRODUCER) / CONSUMER_COUNT;

    while (count < total_items) {
        sem_p(full_sem);   // 等待有物品
        sem_p(mutex_sem);  // 进入临界区

        // 消费物品（从管道读取）
        char buf[16];
        read(pipe_fd[0], buf, 16);
        int item = atoi(buf);
        
        // 记录消费的数据到日志管道
        log_data(consumer_log_fd[1], item);
        printf("Consu %d consumed %d\n", id, item);

        sem_v(mutex_sem);  // 离开临界区
        sem_v(empty_sem);  // 增加空位

        count++;
        sleep(2);  // 模拟消费时间
    }
    printf("Cons %d finished\n", id);
}

// 从日志管道读取所有数据
int read_log_data(int log_fd, int *items, int max_items) {
    char buf[16];
    int count = 0;
    
    // 我们知道确切的生产和消费数量，所以直接读取相应数量的数据
    for (count = 0; count < max_items; count++) {
        int bytes_read = read(log_fd, buf, 16);
        if (bytes_read <= 0) {
            break;  // 没有更多数据
        }
        items[count] = atoi(buf);
    }
    
    return count;
}

// 验证数据匹配
void verify_data_match() {
    printf("\n=== Starting Data Verification ===\n");
    
    int produced_items[PRODUCER_COUNT * ITEMS_PER_PRODUCER];
    int consumed_items[PRODUCER_COUNT * ITEMS_PER_PRODUCER];
    
    // 读取生产日志
    int produced_count = read_log_data(producer_log_fd[0], produced_items, 
                                      PRODUCER_COUNT * ITEMS_PER_PRODUCER);
    
    // 读取消费日志
    int consumed_count = read_log_data(consumer_log_fd[0], consumed_items,
                                      PRODUCER_COUNT * ITEMS_PER_PRODUCER);
    
    // 打印生产的所有项目
    printf("Produced items (%d): ", produced_count);
    for (int i = 0; i < produced_count; i++) {
        printf("%d ", produced_items[i]);
    }
    printf("\n");
    
    // 打印消费的所有项目
    printf("Consumed items (%d): ", consumed_count);
    for (int i = 0; i < consumed_count; i++) {
        printf("%d ", consumed_items[i]);
    }
    printf("\n");
    
    // 检查数量是否匹配
    if (produced_count != consumed_count) {
        printf("ERROR: Item count mismatch! Produced: %d, Consumed: %d\n", 
               produced_count, consumed_count);
        return;
    }
    
    if (produced_count != PRODUCER_COUNT * ITEMS_PER_PRODUCER) {
        printf("ERROR: Expected %d items, but got %d produced items\n",
               PRODUCER_COUNT * ITEMS_PER_PRODUCER, produced_count);
        return;
    }
    
    // 创建生产项目的副本进行匹配
    int produced_copy[PRODUCER_COUNT * ITEMS_PER_PRODUCER];
    for (int i = 0; i < produced_count; i++) {
        produced_copy[i] = produced_items[i];
    }
    
    // 标记已匹配的项目
    int matched[PRODUCER_COUNT * ITEMS_PER_PRODUCER] = {0};
    int all_matched = 1;
    
    // 检查每个消费的项目是否在生产列表中
    for (int i = 0; i < consumed_count; i++) {
        int found = 0;
        for (int j = 0; j < produced_count; j++) {
            if (!matched[j] && consumed_items[i] == produced_copy[j]) {
                matched[j] = 1;
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("ERROR: Consumed item %d was not produced!\n", consumed_items[i]);
            all_matched = 0;
        }
    }
    
    // 检查是否有未消费的项目
    for (int i = 0; i < produced_count; i++) {
        if (!matched[i]) {
            printf("ERROR: Produced item %d was not consumed!\n", produced_copy[i]);
            all_matched = 0;
        }
    }
    
    if (all_matched) {
        printf("SUCCESS: All produced items were correctly consumed!\n");
    } else {
        printf("FAILED: Data mismatch detected!\n");
    }
    
    printf("=== Data Verification Complete ===\n\n");
}

int main() {
    printf("Starting Multi-Producer Multi-Consumer test...\n");

    // 创建主通信管道
    if (pipe(pipe_fd) < 0) {
        printf("ERROR: Failed to create main pipe\n");
        exit(1);
    }

    // 创建生产日志管道
    if (pipe(producer_log_fd) < 0) {
        printf("ERROR: Failed to create producer log pipe\n");
        exit(1);
    }

    // 创建消费日志管道
    if (pipe(consumer_log_fd) < 0) {
        printf("ERROR: Failed to create consumer log pipe\n");
        exit(1);
    }

    // 创建信号量
    empty_sem = sem_create(BUFFER_SIZE);  // 初始空位为缓冲区大小
    full_sem = sem_create(0);             // 初始满位为0
    mutex_sem = sem_create(1);            // 互斥信号量

    if (empty_sem < 0 || full_sem < 0 || mutex_sem < 0) {
        printf("ERROR: Failed to create semaphores\n");
        exit(1);
    }

    // 创建生产者进程
    for (int i = 0; i < PRODUCER_COUNT; i++) {
        if (fork() == 0) {
            // 子进程：关闭不需要的管道端
            close(producer_log_fd[0]);
            close(consumer_log_fd[0]);
            close(consumer_log_fd[1]);
            close(pipe_fd[0]);  // 生产者不需要读取主管道
            producer(i);
            exit(0);
        }
    }

    // 创建消费者进程
    for (int i = 0; i < CONSUMER_COUNT; i++) {
        if (fork() == 0) {
            // 子进程：关闭不需要的管道端
            close(producer_log_fd[0]);
            close(producer_log_fd[1]);
            close(consumer_log_fd[0]);
            close(pipe_fd[1]);  // 消费者不需要写入主管道
            consumer(i);
            exit(0);
        }
    }

    // 父进程：关闭不需要的管道端
    close(producer_log_fd[1]);  // 关闭生产日志的写入端
    close(consumer_log_fd[1]);  // 关闭消费日志的写入端
    close(pipe_fd[0]);          // 关闭主管道的读取端
    close(pipe_fd[1]);          // 关闭主管道的写入端

    // 等待所有子进程结束
    for (int i = 0; i < PRODUCER_COUNT + CONSUMER_COUNT; i++) {
        wait(0);
    }

    // 数据验证
    verify_data_match();

    // 关闭日志管道
    close(producer_log_fd[0]);
    close(consumer_log_fd[0]);

    // 销毁信号量
    sem_destroy(empty_sem);
    sem_destroy(full_sem);
    sem_destroy(mutex_sem);

    printf("MPMC test completed successfully!\n");
    exit(0);
}