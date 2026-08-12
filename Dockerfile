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

FROM debian:bookworm-slim AS runtime

WORKDIR /app

COPY --from=builder /workspace/fem ./fem

CMD ["./fem"]
