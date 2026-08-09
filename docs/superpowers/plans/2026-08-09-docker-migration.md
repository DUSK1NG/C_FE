# Docker Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a reproducible Docker development, test, build, and runtime workflow for the current C11 `c_FE` command-line FEM project without changing its C implementation.

**Architecture:** Use a three-stage Dockerfile. The `development` stage provides GCC with the project directory bind-mounted by Compose, the `builder` stage compiles the application and Stage 1 test and runs the test during image construction, and the `runtime` stage contains only the compiled `fem` executable. Compose exposes `dev` for interactive development and `app` for the small runtime image.

**Tech Stack:** Docker Engine, Docker Compose, GCC 13 on Debian Bookworm, Debian Bookworm slim runtime, C11.

## Global Constraints

- Preserve the existing C source, header files, algorithm behavior, and Stage 1 test.
- Compile with `-std=c11 -Wall -Wextra -pedantic -lm`.
- Keep the project root as the Docker build context.
- Do not introduce CMake, Make, a database, ports, or persistent volumes in this migration.
- The formal image must run the existing Stage 1 test before the runtime image is produced.
- The formal runtime image must not contain GCC, source files, or the test executable.

## File Map

- Create: `Dockerfile` — multi-stage development, builder, and runtime image definition.
- Create: `compose.yaml` — interactive `dev` service and one-shot `app` service.
- Create: `.dockerignore` — excludes repository metadata and generated host artifacts from the build context.
- Create: `docs/superpowers/plans/2026-08-09-docker-migration.md` — this implementation plan.
- Modify: no existing C source or header files.

### Task 1: Add the multi-stage Dockerfile

**Files:**
- Create: `Dockerfile`
- Test: Docker builder and runtime targets

**Interfaces:**
- Consumes: `src/main.c`, `src/fem.c`, `include/`, and `tests/test_stage1.c` from the existing project.
- Produces: `/workspace/fem` and `/workspace/test_stage1` in the builder stage; `/app/fem` in the runtime stage.

- [x] **Step 1: Create the Dockerfile with the development stage**

```dockerfile
# syntax=docker/dockerfile:1

FROM gcc:13-bookworm AS development

WORKDIR /workspace

CMD ["sh"]
```

The development stage intentionally does not copy source files. Compose supplies the current project through a bind mount, so source changes are immediately visible inside the container.

- [x] **Step 2: Add the builder stage and existing compile commands**

Append this exact stage to `Dockerfile`:

```dockerfile
FROM gcc:13-bookworm AS builder

WORKDIR /workspace

COPY include ./include
COPY src ./src
COPY tests ./tests

RUN gcc -std=c11 -Wall -Wextra -pedantic \
        src/main.c src/fem.c -Iinclude -o fem -lm

RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage1.c src/fem.c -Iinclude -o test_stage1 -lm

RUN ./test_stage1
```

The final `RUN` must remain in the builder stage so a failing regression test stops the image build.

- [x] **Step 3: Add the minimal runtime stage**

Append this exact stage to `Dockerfile`:

```dockerfile
FROM debian:bookworm-slim AS runtime

WORKDIR /app

COPY --from=builder /workspace/fem ./fem

CMD ["./fem"]
```

Only the main executable crosses the stage boundary; the test executable, compiler, headers, and source do not.

### Task 2: Add Compose services and build-context exclusions

**Files:**
- Create: `compose.yaml`
- Create: `.dockerignore`
- Test: `docker compose config`

**Interfaces:**
- Consumes: Dockerfile targets named `development` and `runtime`.
- Produces: `dev` for an interactive bind-mounted toolchain and `app` for the final image.

- [x] **Step 1: Create the Compose file**

```yaml
services:
  dev:
    build:
      context: .
      target: development
    working_dir: /workspace
    volumes:
      - .:/workspace
    stdin_open: true
    tty: true

  app:
    build:
      context: .
      target: runtime
    image: c-fe:fem
```

The `dev` service uses the host project directory as `/workspace`; the `app` service builds the runtime target and does not expose ports or create data volumes.

- [x] **Step 2: Create the Docker build-context exclusions**

```text
.git
.gitignore
.vscode
.worktrees
docs
*.exe
*.o
fem
test_stage1
build
dist
```

Keep `src`, `include`, and `tests` in the build context because the builder stage copies them explicitly.

- [ ] **Step 3: Validate the Compose model**

Run:

```powershell
docker compose config
```

Expected: Compose prints a normalized configuration containing `dev` and `app`, with no validation error.

Current result: blocked because the current environment does not have the `docker` command installed.

### Task 3: Build, test, run, and verify migration behavior

**Files:**
- Modify: none
- Test: Docker image build, runtime execution, development-container compilation, and repository diff

**Interfaces:**
- Consumes: `Dockerfile`, `compose.yaml`, `.dockerignore`, and the existing C Stage 1 code.
- Produces: a passing `c-fe:fem` image and verified developer commands for another PC.

- [ ] **Step 1: Check Docker availability**

Run:

```powershell
docker --version
docker compose version
```

Expected: both commands return installed Docker and Compose versions. If either command is unavailable, start Docker Desktop or install Docker Desktop before continuing.

Current result: blocked because `docker` is not available in the current environment.

- [ ] **Step 2: Build the formal image and execute the builder test**

Run:

```powershell
docker compose build app
```

Expected: the build succeeds and its logs contain:

```text
Stage 1 tests passed.
```

- [ ] **Step 3: Run the formal image**

Run:

```powershell
docker compose run --rm app
```

Expected output contains:

```text
Stage 1: single 2D truss element
Length = 943.398113...
c = 0.529998940...
s = 0.847998304...
Element stiffness matrix [N/mm]:
```

- [ ] **Step 4: Verify the bind-mounted development workflow**

Run:

```powershell
docker compose run --rm dev sh -c "gcc -std=c11 -Wall -Wextra -pedantic src/main.c src/fem.c -Iinclude -o /tmp/fem -lm && gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage1.c src/fem.c -Iinclude -o /tmp/test_stage1 -lm && /tmp/test_stage1 && /tmp/fem"
```

Expected: the command exits successfully, prints `Stage 1 tests passed.`, and then prints the main program output. `/tmp` keeps development binaries out of the mounted project directory.

- [x] **Step 5: Verify the final diff and repository status**

Run:

```powershell
git diff --check
git status --short
```

Expected: no whitespace errors; only the planned Docker files and this plan/implementation change are present. No C source or header file is modified.

- [x] **Step 6: Commit the implementation**

```powershell
git add Dockerfile compose.yaml .dockerignore
git commit -m "build: add Docker workflow for c_FE"
```

Expected: one implementation commit containing only the three Docker migration files.

## Self-Review Checklist

- Spec coverage: the three image stages, two Compose services, no ports or volumes, automatic Stage 1 testing, migration commands, and runtime minimization are covered by Tasks 1–3.
- Placeholder scan: no `TBD`, `TODO`, or unspecified implementation step is used.
- Interface consistency: Compose targets exactly match the `development` and `runtime` Dockerfile stages; the builder output copied by runtime is exactly `fem`.
- Test consistency: the build uses the same source files and compiler flags as the existing project command, and the development verification uses `/tmp` so it does not rely on host compiler output.

## Execution Status

The Dockerfile, Compose file, and Docker ignore file are committed. Static checks passed, and the working tree is clean. Docker Compose parsing, image building, runtime execution, and bind-mounted development execution remain pending until Docker Desktop is installed and running.
