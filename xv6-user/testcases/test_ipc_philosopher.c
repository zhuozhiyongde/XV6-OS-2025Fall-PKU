#include "test.h"

#define PHILOSOPHER_COUNT 5
#define EAT_COUNT 2

int chopstick_sem[PHILOSOPHER_COUNT];

void philosopher(int id, int write_fd) {
    int left = id;
    int right = (id + 1) % PHILOSOPHER_COUNT;

    for (int i = 0; i < EAT_COUNT; i++) {
        // printf("Ph %d is thinking... (round %d)\n", id, i + 1);
        sleep(10);  // 思考

        // 避免死锁：编号为偶数的哲学家先拿左边，奇数先拿右边
        if (id % 2 == 0) {
            sem_p(chopstick_sem[left]);
            // printf("Ph %d picked up left %d\n", id, left);
            sleep(5);

            sem_p(chopstick_sem[right]);
            // printf("Ph %d picked up right %d\n", id, right);
        } else {
            sem_p(chopstick_sem[right]);
            // printf("Ph %d picked up right %d\n", id, right);
            sleep(5);

            sem_p(chopstick_sem[left]);
            // printf("Ph %d picked up left %d\n", id, left);
        }

        // 就餐
        // printf("Ph %d is eating... (round %d)\n", id, i + 1);
        sleep(5);  // 就餐

        // 放回筷子
        sem_v(chopstick_sem[left]);
        sem_v(chopstick_sem[right]);
        // printf("Ph %d finished and put down\n", id);
        
        // 每次就餐完成后，通过pipe通知父进程
        char buf[1] = {1};  // 发送1表示一次就餐完成
        write(write_fd, buf, 1);
    }

    printf("Ph %d finished all meals\n", id);
    close(write_fd);  // 关闭写端
}

int main() {
    printf("Starting Dining Philosophers test...\n");

    // 创建管道用于子进程向父进程报告就餐次数
    int pipe_fds[PHILOSOPHER_COUNT][2];
    int eat_count[PHILOSOPHER_COUNT] = {0};  // 记录每个哲学家的就餐次数
    
    for (int i = 0; i < PHILOSOPHER_COUNT; i++) {
        if (pipe(pipe_fds[i]) < 0) {
            printf("ERROR: Failed to create pipe for philosopher %d\n", i);
            exit(1);
        }
    }

    // 创建筷子信号量
    for (int i = 0; i < PHILOSOPHER_COUNT; i++) {
        chopstick_sem[i] = sem_create(1);  // 每根筷子初始为1（可用）
        if (chopstick_sem[i] < 0) {
            printf("ERROR: Failed to create chopstick semaphore %d\n", i);
            exit(1);
        }
    }

    // 创建哲学家进程
    for (int i = 0; i < PHILOSOPHER_COUNT; i++) {
        if (fork() == 0) {
            // 子进程：关闭读端，使用写端
            close(pipe_fds[i][0]);
            philosopher(i, pipe_fds[i][1]);
            exit(0);
        } else {
            // 父进程：关闭写端，保留读端
            close(pipe_fds[i][1]);
        }
    }

    // 从所有管道读取就餐次数
    for (int i = 0; i < PHILOSOPHER_COUNT; i++) {
        char buf[1];
        int count = 0;
        
        // 读取直到管道关闭（EOF）
        while (read(pipe_fds[i][0], buf, 1) > 0) {
            eat_count[i]++;
            count++;
        }
        
        close(pipe_fds[i][0]);  // 关闭读端
        printf("Philosopher %d ate %d times\n", i, count);
    }

    // 等待所有哲学家完成
    for (int i = 0; i < PHILOSOPHER_COUNT; i++) {
        wait(0);
    }

    // 验证所有哲学家都完成了预期的就餐次数
    int all_correct = 1;
    for (int i = 0; i < PHILOSOPHER_COUNT; i++) {
        if (eat_count[i] != EAT_COUNT) {
            printf("ERROR: Philosopher %d only ate %d times, expected %d\n", 
                   i, eat_count[i], EAT_COUNT);
            all_correct = 0;
        }
    }
    
    if (all_correct) {
        printf("SUCCESS: All philosophers completed exactly %d meals each!\n", EAT_COUNT);
    } else {
        printf("ERROR: Not all philosophers completed the expected number of meals!\n");
    }

    // 销毁信号量
    for (int i = 0; i < PHILOSOPHER_COUNT; i++) {
        sem_destroy(chopstick_sem[i]);
    }

    printf("Dining Philosophers test completed!\n");
    exit(all_correct ? 0 : 1);
}