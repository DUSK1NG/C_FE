# syntax=docker/dockerfile:1

FROM gcc:13-bookworm AS development

WORKDIR /workspace

CMD ["sh"]

FROM gcc:13-bookworm AS builder

WORKDIR /workspace

COPY include ./include
COPY src ./src
COPY tests ./tests

RUN gcc -std=c11 -Wall -Wextra -pedantic \
        src/main.c src/fem.c src/solver.c -Iinclude -o fem -lm

RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage1.c src/fem.c src/solver.c -Iinclude -o test_stage1 -lm

RUN ./test_stage1

RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage6.c src/postprocess.c -Iinclude -o test_stage6 -lm

RUN ./test_stage6

RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage7.c src/fem.c src/solver.c src/reactions.c \
        -Iinclude -o test_stage7 -lm

RUN ./test_stage7

RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage8.c src/fem.c src/solver.c src/reactions.c src/io.c \
        -Iinclude -o test_stage8 -lm

RUN ./test_stage8

RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage9.c src/fem.c src/solver.c src/reactions.c \
        src/postprocess.c src/io.c src/output.c \
        -Iinclude -o test_stage9 -lm

RUN ./test_stage9 > stage9.out && \
    ./test_stage9 --emit-debug >> stage9.out && \
    grep -F "Stage 9 results output contract tests passed." stage9.out && \
    grep -F "K_original" stage9.out && \
    grep -F "F_original" stage9.out

RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage10.c src/fem.c src/solver.c src/reactions.c \
        src/postprocess.c src/io.c src/output.c \
        -Iinclude -o test_stage10 -lm

RUN ./test_stage10

FROM debian:bookworm-slim AS runtime

WORKDIR /app

COPY --from=builder /workspace/fem ./fem

CMD ["./fem"]
