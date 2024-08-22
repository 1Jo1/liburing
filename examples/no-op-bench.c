/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/wait.h>

#include "liburing.h"

static unsigned long runtime_ms = 10000;

static unsigned long gettimeofday_ms(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

int main(void)
{
	unsigned long tstop;
	unsigned long nr_reqs = 0;
	struct io_uring_cqe *cqe;
	struct io_uring_sqe *sqe;
	struct io_uring ring;
	int ret, i, qd = 64;

	ret = io_uring_queue_init(1024, &ring, IORING_SETUP_SINGLE_ISSUER);
	if (ret == -EINVAL) {
		fprintf(stderr, "can't single\n");
		ret = io_uring_queue_init(1024, &ring, 0);
	}
	if (ret) {
		fprintf(stderr, "child: ring setup failed: %d\n", ret);
		return 1;
	}

	tstop = gettimeofday_ms() + runtime_ms;
	do {
		for (i = 0; i < qd; i++) {
			sqe = io_uring_get_sqe(&ring);
			io_uring_prep_nop(sqe);
			sqe->user_data = 1;
		}

		ret = io_uring_submit(&ring);
		if (ret != qd) {
			fprintf(stderr, "child: sqe submit failed: %d\n", ret);
			return 1;
		}

		for (i = 0; i < qd; i++) {
			ret = io_uring_wait_cqe(&ring, &cqe);
			if (ret < 0) {
				fprintf(stderr, "child: wait completion %d\n", ret);
				break;
			}
			io_uring_cqe_seen(&ring, cqe);
			nr_reqs++;
		}
	} while (gettimeofday_ms() < tstop);

	fprintf(stderr, "requests/s: %lu\n", nr_reqs * 1000UL / runtime_ms);
	return 0;
}
