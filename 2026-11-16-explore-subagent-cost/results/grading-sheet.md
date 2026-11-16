## ky-retry-flow

Trace what happens inside ky when a GET request gets a 503 response and retries are enabled. Name the internal methods involved, in order, and say when the beforeRetry hooks run.

Points:
1. The request goes run -> retry -> fetch (the private #run, #retry and #fetch methods, or equivalent names)
2. afterResponse hooks run on the 503 response before ky decides to throw or retry
3. A HTTPError is created and the retry decision (#retryFromError / #calculateRetryDelay) checks the retry limit, the method and the status code list
4. The delay comes from the Retry-After style header for statuses in afterStatusCodes (capped by maxRetryAfter), otherwise from retry.delay (0.3 s doubling) with optional jitter, capped by backoffLimit
5. ky waits for the delay first; beforeRetry hooks run after the delay and before the next fetch, then the retry count goes up and fetch runs again

### ky-retry-flow/06bc

A 503 on a GET goes through these steps in `source/core/Ky.ts`:

1. `Ky.create()` builds the instance and calls `#run()`.
2. `#run()` runs `#runBeforeRequestHooks()`, then calls `#retry()`.
3. `#retry()` calls `#fetch()`. `#fetch()` clones the request for a possible retry and sends it. A 503 does not throw there. It returns the `Response`.
4. Back in `#run()`'s loop, `#runAfterResponseHooks()` runs. The response is not `ok`, so `#shouldThrowHttpErrors()` is true. Ky builds an `HTTPError`, calls `#getResponseData()`, and passes the error to `#retryFromError()`.
5. `#retryFromError()` calls `#calculateRetryDelay()`, which checks the retry limit, the `shouldRetry` option, the status codes and `Retry-After`. It uses `#calculateDelay()` for the backoff. It then waits with `delay()`.
6. **The `beforeRetry` hooks run here.** They run in order, after the delay and before the next attempt. This is also after the `afterResponse` hooks for the failed response.
7. `#retryFromError()` increments `#retryCount` and calls `#retry()` again. That calls `#fetch()` again and returns to the `#run()` loop.

A `beforeRetry` hook can stop the retry or replace the request, the options or the response.

### ky-retry-flow/60ae

For a GET that gets a 503 with retries enabled, the path in `source/core/Ky.ts` is below. The Explore agent read the code but did not run it.

1. `Ky.create` builds the instance and runs `#run`.
2. `#run` runs `#runBeforeRequestHooks`, then `#retry`.
3. `#retry` calls `#fetch`, which returns the 503 Response without throwing.
4. Back in `#run`, `#runAfterResponseHooks` runs. Because the response isn't ok, it builds an `HTTPError` and calls `#retryFromError`.
5. `#retryFromError` calls `#calculateRetryDelay`. That checks the retry limit, `retry.methods`, `shouldRetry`, `retry.statusCodes` (503 is a default), and `Retry-After`. It then calls `#calculateDelay` for backoff and jitter, or rethrows the error to stop retrying.
6. `#retryFromError` checks the remaining total timeout, then sleeps for the delay.
7. The `beforeRetry` hooks run after that sleep and before the next fetch. A hook can replace the request, return a Response, or return `ky.stop`.
8. `#retryCount` is incremented, then `#retry` runs again.

If retries run out, `#throwProcessedError` runs the `beforeError` hooks.

### ky-retry-flow/035b

On a GET that returns 503 with retries enabled, `Ky.create` builds the request and `#run()` drives the flow (`source/core/Ky.ts`):

1. `#run()` runs `#runBeforeRequestHooks()`, then calls `#retry()`.
2. `#retry()` calls `#fetch()`. That clones the request for a possible retry, wraps it with `#wrapRequestWithUploadProgress`, and sends it through `fetch` (via `timeout()` if a timeout is set). It returns the 503 response with `#setResponseRequest`.
3. Back in `#run()`'s loop, `#runAfterResponseHooks()` runs. Because `response.ok` is false and `#shouldThrowHttpErrors` is true, it builds an `HTTPError` and fills `httpError.data` via `#getResponseData()`.
4. `#run()` then calls `#retryFromError(httpError)`.
5. `#retryFromError` calls `#calculateRetryDelay()`. That checks the retry limit, the method, the status code and the `shouldRetry` option.
6. It waits with `delay()`, and resets the abort controller if needed.
7. The `beforeRetry` hooks run next. They run after the delay and any forced-retry request swap, and before the next attempt. They receive `retryCount + 1`.
8. It increments `#retryCount` and calls `#retry()` again, which calls `#fetch()`.

If the limit is exhausted, the `HTTPError` is thrown through `#throwProcessedError`.

### ky-retry-flow/ba68

On a 503 GET, `Ky.create` wraps `#run()` and runs `#runBeforeRequestHooks` and then `#retry()`. That calls `#fetch()`, which returns the 503 without throwing. Back in `#run`, `#runAfterResponseHooks` runs. Because the response isn't ok and `#shouldThrowHttpErrors` is true, `#run` builds an `HTTPError` and calls `#retryFromError`.

`#retryFromError` calls `#calculateRetryDelay`, which checks:
- the retry limit
- `retry.methods`
- `shouldRetry`
- `retry.statusCodes`
- a `Retry-After` header
- `#calculateDelay` for backoff

`#retryFromError` then waits with `delay()`.

**The `beforeRetry` hooks run after that delay.** They run sequentially, with `#retryCount` still at its old value (it is passed to them as `retryCount + 1`). Afterwards `#retryCount++` runs and `#retry()` calls `#fetch()` again.

If `#calculateRetryDelay` throws, no `beforeRetry` hook runs. `create`'s catch then calls `#throwProcessedError`, which runs the `beforeError` hooks.

I haven't opened `source/core/Ky.ts` myself. This trace is from the Explore agent's read of that file.

### ky-retry-flow/7d6c

On a 503 GET with retries enabled, ky runs these methods in `source/core/Ky.ts`:

1. `Ky.create` builds the instance and awaits `#run()`.
2. `#run` runs the `beforeRequest` hooks, then calls `#retry()`.
3. `#retry` calls `#fetch()`, which resolves with the 503 `Response`. It doesn't throw.
4. `#runAfterResponseHooks` runs on the 503.
5. Back in `#run`, the response isn't ok, so ky builds an `HTTPError`.
6. `#retryFromError` takes that error:
   - `#calculateRetryDelay` checks the retry limit, the method, `shouldRetry` and whether 503 is a retriable status.
   - It honors a `Retry-After` header, since 503 is in `afterStatusCodes`. Otherwise it calls `#calculateDelay` for the backoff.
   - `#retryFromError` then sleeps for that delay.
7. The `beforeRetry` hooks run here, **after the sleep and before the next fetch**. They can:
   - replace the request
   - return a `Response`
   - return `ky.stop`
   - change options through `#applyBeforeRetryOptions`
8. `#retryFromError` increments `#retryCount` and calls `#retry()` again, which calls `#fetch` again.

If retries are exhausted, `#calculateRetryDelay` throws the `HTTPError`. `beforeRetry` never runs in that case. The error goes through `#throwProcessedError`, which runs the `beforeError` hooks.

### ky-retry-flow/dfcd

Flow, in `source/core/Ky.ts`:

1. `Ky.create()` builds the instance. Its internal request runner (the `Ky` constructor's promise, around line 598) calls `#retry()`.
2. `#retry()` calls `#fetch()`, which sends the request. It also prepares a cloned request for a possible retry.
3. A 503 does not throw from `fetch`. The loop around line 600 runs `#runAfterResponseHooks()`. Because `response.ok` is false, it builds an `HTTPError` and reads its body with `#getResponseData()`.
4. The loop calls `#retryFromError(httpError)`.
5. `#calculateRetryDelay()` checks the retry limit, method, status code and `shouldRetry`, then calls `#calculateDelay()` (backoff, or `Retry-After`). If it decides not to retry, it throws.
6. `#retryFromError` waits with `delay()`, checks total-timeout, and resets the abort controller if needed.
7. **The `beforeRetry` hooks run here**, sequentially, after the delay and before the next attempt. A hook can return a `Request`, a `Response`, `ky.stop`, or an options object, which goes through `#applyBeforeRetryOptions()`.
8. `#retryCount++`, then `#retry()` again, which calls `#fetch()`.

If retries run out, the error goes to `#throwProcessedError()`, which runs the `beforeError` hooks.

### ky-retry-flow/b6b0

On a 503 GET with retries enabled, ky goes through these steps. The code is in `source/core/Ky.ts`.

1. `Ky.create()` builds the instance and runs the main request flow, which in this version is the async `#run`-style method around line 598.
2. It calls `#retry()`, which calls `#fetch()`. `#fetch()` clones the request for a possible retry and dispatches it. Before that, `#runBeforeRequestHooks()` has already run once.
3. The 503 response goes through `#runAfterResponseHooks()`.
4. The response is not `ok`, so `#shouldThrowHttpErrors()` is true. Ky builds an `HTTPError` and fills `data` through `#getResponseData()`.
5. The loop then calls `#retryFromError(httpError)`.
6. `#retryFromError()` calls `#calculateRetryDelay()`, which checks the retry limit, the method, the status code and `shouldRetry`. It returns a delay through `#calculateDelay()` or `Retry-After`, or it throws.
7. `#retryFromError()` waits with `delay()` and checks the total timeout.
8. **The `beforeRetry` hooks run here.** They run after the delay and after the abort-controller reset or custom-request step. They run before `#retryCount++` and before the next `#retry()` → `#fetch()`.
9. Hook options are applied through `#applyBeforeRetryOptions()`.

### ky-retry-flow/83bd

For a GET that returns 503, ky runs these steps in `source/core/Ky.ts`:

1. `Ky.create` runs the `init` hooks and then calls `#run`.
2. `#run` runs the `beforeRequest` hooks via `#runBeforeRequestHooks`, then makes the first attempt with `#retry`.
3. `#retry` calls `#fetch`. The 503 comes back as a normal `Response`, not a thrown error.
4. Back in `#run`'s `for (;;)` loop, `#runAfterResponseHooks` runs first. Because `response.ok` is false, ky builds an `HTTPError` and reads its body with `#getResponseData`.
5. `#run` calls `#retryFromError`. That method calls `#calculateRetryDelay`, which checks the limit, method and status code, honors `Retry-After`, and otherwise uses `#calculateDelay` for backoff.
6. `#retryFromError` then sleeps with `delay()`.
7. The `beforeRetry` hooks run here, after the sleep and before the next fetch. A hook can replace the request or response, change options, or return `stop`.
8. `#retryFromError` increments `#retryCount` and returns `#retry()`, and the loop repeats until the response is OK.
9. `#decorateResponse` runs on the final response.

The retry is a loop in `#run`, not recursion. The agent did not check the default retry limit, status codes or methods, so I haven't confirmed that 503 and GET are retried by default.

## ky-timeout

How does ky enforce the `timeout` option on a single attempt, and what happens to the timeout when the request is retried?

Points:
1. A timeout() helper races fetch against a setTimeout timer
2. When the timer fires it aborts ky's AbortController and rejects with a TimeoutError
3. Each attempt gets its own timeout (the timer is cleared in finally), limited by the remaining totalTimeout budget when one is set
4. TimeoutError is only retried when retryOnTimeout is enabled (or shouldRetry returns true); by default it is not
5. Before a retry after a timeout, the aborted AbortController is replaced with a fresh one (new managed signal) so the next attempt is not born aborted

### ky-timeout/eea7

**Single attempt:** `#fetch()` in `source/core/Ky.ts` calls `#getEffectiveTimeout()`, then passes the result to `timeout()` in `source/utils/timeout.ts`. That function starts a `setTimeout` and races it against `fetch`. When the timer fires, it aborts the `AbortController` and rejects with `TimeoutError`. A late response is discarded and its body cancelled. A `finally` calls `clearTimeout`, so the timer can't leak into the next attempt. If `timeout` is `false` and there is no `totalTimeout`, plain `fetch` runs with no timer.

**Retry:** `timeout` is per attempt. `#retry()` recurses into `#fetch()`, which calls `timeout()` again, so each attempt gets a full fresh timer. After a timeout abort, `#retry()` creates a new `AbortController` and rebuilds the managed signal. A `beforeRetry` hook can change `timeout` through `#applyBeforeRetryOptions`.

The separate `totalTimeout` option is the exception. It sets `#deadline` once and is never reset. `#getEffectiveTimeout()` caps each attempt at the remaining budget.

### ky-timeout/68e1

**Single attempt:** `#fetch()` in `source/core/Ky.ts` calls `#getEffectiveTimeout()`. That returns `min(timeout, remaining totalTimeout)`, or `undefined` if there's no limit, in which case it calls `fetch` directly. Otherwise it calls `timeout()` in `source/utils/timeout.ts`. That function starts a `setTimeout` racing the `fetch`. When the timer fires, it calls `abortController.abort()` and rejects with `TimeoutError`. A late response is discarded, and `clearTimeout` runs in `finally`.

**On retry:**
- `#retryFromError()` computes the retry delay and waits with `delay()`. That wait is outside the per-attempt timer, so it doesn't count against `timeout`. It does count against `totalTimeout`.
- The timeout is reset for each attempt, because each `#fetch()` call starts a new timer. Later attempts can get a shorter timer if `totalTimeout` is nearly used up.
- If the previous attempt aborted the controller (as a timeout does), `#retryFromError()` creates a new `AbortController` and rebuilds the signal.
- A `TimeoutError` is retried only if `retry.retryOnTimeout` is true or `shouldRetry` returns `true`.

### ky-timeout/5e32

Ky enforces `timeout` per attempt, and each retry gets a fresh timeout.

**Enforcement:** `timeout()` in `source/utils/timeout.ts` wraps a single `fetch`. It starts a `setTimeout`. When the timer fires, it aborts the internal `AbortController` and rejects with `TimeoutError`. It clears the timer in `finally`. `#fetch()` in `Ky.ts` calls it, passing the value from `#getEffectiveTimeout()`.

**Retries:** `#retry()` calls `#fetch()`. On failure, `#retryFromError()` decides whether to retry through `#calculateRetryDelay()`, which includes the `retryOnTimeout` check. It then waits with `delay()` and runs the `beforeRetry` hooks. Each new `#fetch()` starts a full `options.timeout` timer. Because a timeout aborts the internal controller, `#retryFromError()` creates a new `AbortController` and rebuilds the managed signal with `#createManagedSignal()`. A `beforeRetry` hook can change `timeout` for the next attempt via `#applyBeforeRetryOptions()`.

**Overall cap:** the optional `totalTimeout` limits the whole request. `#getEffectiveTimeout()` caps each attempt at `min(timeout, remaining totalTimeout)`.

### ky-timeout/74a6

I've read the code and I'm writing up the answer now.

**Per-attempt enforcement:** `Ky#fetch()` calls `#getEffectiveTimeout()`. That returns `min(options.timeout, remaining totalTimeout)`, or `undefined` if both are unbounded. It then calls `timeout()` in `utils/timeout.ts`.

`timeout()` starts a `setTimeout` and calls `fetch(request, init)` inside a Promise. If the timer fires first, it calls `abortController.abort()` and rejects with `TimeoutError`. If a response arrives late, `cancelBody()` discards it. A `finally` block runs `clearTimeout`, so a stale timer can't abort a later attempt.

**On retry:** `#retry()` calls `#fetch()` again, so each attempt gets a fresh `timeout` window. It is not a shared budget. `isTimeoutError` plus `retry.retryOnTimeout` decide whether a timeout is retried. The only cross-attempt cap is `totalTimeout`, a deadline fixed at construction. `#getRemainingTotalTimeout()` shrinks each attempt's window, and `#fetch` throws `TimeoutError` once it reaches 0. A `beforeRetry` hook can change `timeout` for later attempts via `#applyBeforeRetryOptions`.

### ky-timeout/02f6

**Enforcing it on one attempt:** `#fetch()` in `source/core/Ky.ts` runs once per attempt. It calls `#getEffectiveTimeout()`, which returns the smaller of `timeout` and the remaining `totalTimeout`. It then passes that value to `timeout()` in `source/utils/timeout.ts`. That function races `fetch` against a `setTimeout`. When the timer fires, it aborts the attempt's `AbortController` and rejects with `TimeoutError`. A `finally` block calls `clearTimeout`.

**On retry:**
- `#retry()` calls `#fetch()` again, so each attempt gets a fresh `timeout`.
- Before that, `#retryFromError()` replaces the aborted controller with a new one, using `#createManagedSignal()`. This keeps the next attempt from starting already aborted.
- A timeout is retried only if `retry.retryOnTimeout` is set or `shouldRetry` returns `true`, as decided in `#calculateRetryDelay()`.
- `totalTimeout` is the only budget shared across attempts. It is a `#deadline` that is set once and never reset. Retry delays and `beforeRetry` hooks count against it, but they are not covered by the per-attempt timer.

### ky-timeout/0364

**Per attempt:** `#fetch()` in `source/core/Ky.ts` calls the `timeout()` helper in `source/utils/timeout.ts`. That helper does three things:
- It starts a `setTimeout` and races it against `options.fetch(request, init)`.
- When the timer fires, it calls `abortController.abort()` and rejects with `TimeoutError`.
- A `finally` block runs `clearTimeout`, so a stale timer can't abort a later attempt. If a response arrives after the timeout won, `cancelBody()` discards it.

**On retry:** `#retry()` and `#retryFromError()` handle it.
- `timeout` is per attempt, so each retry gets a fresh full timer. A `TimeoutError` is retried only if `retry.retryOnTimeout` is set, which the check near line 726 enforces.
- `#retryFromError()` waits out the delay. If the internal `AbortController` was aborted by the timeout, it creates a new one and rebuilds the signal with `#createManagedSignal()` and `#withManagedSignal()`.
- The separate `totalTimeout` option spans all attempts. `#getRemainingTotalTimeout()` and `#throwIfTotalTimeoutExhausted()` enforce it, and they stop retrying when the budget runs out.

### ky-timeout/6d14

Ky enforces `timeout` per attempt in `#fetch()` (`source/core/Ky.ts`). It calls `#getEffectiveTimeout()`, which returns the smaller of `timeout` and the remaining `totalTimeout` budget. It returns `undefined` when neither applies, and then `fetch` runs with no timer.

Otherwise `#fetch()` calls `timeout()` in `source/utils/timeout.ts`. That function:
- starts a `setTimeout` and calls `fetch(request, init)`, with the two racing inside a `Promise`;
- on expiry, calls `abortController.abort()` and rejects with `TimeoutError`;
- cancels the body of a response that arrives after the timeout, via `cancelBody()`;
- clears the timer in a `finally` block, so it can't abort a later attempt.

On retry, `#fetch()` runs again and computes a fresh `timeout`, so each attempt gets the full per-attempt value. `totalTimeout` is the exception: it is a fixed `#deadline` shared across all attempts. `#getEffectiveTimeout()` shrinks the timeout to the remaining budget, and `#getRemainingTotalTimeout()` returning 0 throws `TimeoutError`. Retry decisions on a `TimeoutError` depend on `retry.retryOnTimeout`.

### ky-timeout/ca83

**Per attempt:** `#fetch()` in `core/Ky.ts` (around line 1244) computes `effectiveTimeout` via `#getEffectiveTimeout()`. That is the smaller of `timeout` and the remaining `totalTimeout` budget. It then calls `timeout()` in `utils/timeout.ts`.

`timeout()` wraps the `fetch(request, init)` call in a promise. It starts a `setTimeout` that:
- calls `abortController.abort()`,
- rejects with `TimeoutError`.

If a response arrives late, `cancelBody()` discards it. A `finally` block calls `clearTimeout`, so a stale timer can't abort a later attempt.

**On retry:** `#retry()` and `#retryFromError()` handle the retry. A `TimeoutError` goes through the normal retry checks, including `shouldRetry`. `#retryFromError()` then replaces the aborted `AbortController` with a fresh one. The retried request therefore gets a new signal, and `#fetch()` calls `timeout()` again. Each attempt gets a full `timeout`, not what's left of the previous one. Only `totalTimeout` is shared across attempts. `#raceWithTotalTimeout` enforces it, and `#getEffectiveTimeout()` caps each attempt by what remains.

## ky-hooks-order

In what order do ky's hooks run during one request that keeps failing with an HTTP error until the retries run out, and which hooks can replace the response?

Points:
1. init hooks run first, synchronously, on the options before the request is built
2. beforeRequest hooks run once before the first fetch; returning a Response skips the fetch (and that response is not retried)
3. afterResponse hooks run after every response, including each retry; they can return a new Response or force a retry with ky.retry()
4. beforeRetry hooks run before each retry attempt; they can return a Request, a Response, or ky.stop
5. beforeError hooks run last, once, when the final HTTPError is thrown; they can return a modified error

### ky-hooks-order/c91a

In ky (`source/core/Ky.ts`), a request that keeps returning an HTTP error runs its hooks in this order:

1. **`init`** hooks run in `Ky.create`, before the constructor.
2. **`#run`** calls `#runBeforeRequestHooks`.
3. **`#retry`** calls `#fetch` for each attempt.
4. **`#runAfterResponseHooks`** runs after each response. The 500 then becomes an `HTTPError`.
5. **`#retryFromError`** calls `#calculateRetryDelay`, which throws once the retry limit is reached. Otherwise it applies the delay and runs the **`beforeRetry`** hooks.
6. Steps 3–5 repeat for each retry.
7. After the last attempt, **`#throwProcessedError`** runs **`beforeError`** once. It never runs per attempt.

**Hooks that can replace the response:**
- `beforeRequest` can return a `Response`, which skips the fetch.
- `afterResponse` can return a `Response`.
- `beforeRetry` can return a `Response`, which then goes back through `afterResponse`.

`beforeError` can only replace the error. `ky.stop` in `beforeRetry` ends the request with `undefined`, so `beforeError` doesn't run.

### ky-hooks-order/3afc

**Order** (all in `source/core/Ky.ts`):

1. `init` hooks run once in `Ky.create`.
2. `#run` calls `#runBeforeRequestHooks`, which runs once only.
3. `#retry` calls `#fetch`. The 500 response comes back.
4. `#runAfterResponseHooks` runs on every attempt, before the `HTTPError` is built in `#run`.
5. `#retryFromError` calls `#calculateRetryDelay`, which checks the limit, `shouldRetry` and status codes. It then waits and runs the `beforeRetry` hooks, increments the retry count and calls `#retry` again.
6. Steps 3–5 repeat. `init` and `beforeRequest` don't run again.
7. Once retries run out, `#calculateRetryDelay` throws. `#throwProcessedError` then runs the `beforeError` hooks once.

**Hooks that can replace the response:**
- `beforeRequest`: a returned `Response` skips the fetch. It still goes through `afterResponse`, and a failing one throws without a retry.
- `beforeRetry`: a returned `Response` stands in for the next fetch.
- `afterResponse`: a returned `Response` replaces the current one.

`beforeError` can only replace the error. `init` replaces nothing.

### ky-hooks-order/54f8

Order, for one request that keeps failing with an HTTP error until retries run out (`core/Ky.ts`):

1. `init` hooks run synchronously in `Ky.create`, before the `Ky` instance is built.
2. `#runBeforeRequestHooks` runs once, before the first fetch.
3. `#retry` → `#fetch` sends the request.
4. `#runAfterResponseHooks` runs on the response.
5. `#run` sees `!ok` and builds an `HTTPError`, then calls `#retryFromError`. That function runs `#calculateRetryDelay` (including `shouldRetry`), waits, then runs the `beforeRetry` hooks.
6. `#retry` fetches again, and steps 4–6 repeat for each attempt.
7. When `#calculateRetryDelay` finds the retry limit exhausted, it throws. `#throwProcessedError` then runs the `beforeError` hooks, and the final error is thrown.

`beforeRequest` does not run again on retries.

**Hooks that can replace the response:**
- `beforeRequest`: returning a `Response` skips the fetch.
- `afterResponse`: returning a `Response` replaces the current one.
- `beforeRetry`: returning a `Response` replaces it and skips the retry fetch.

`beforeError` can only replace the error.

### ky-hooks-order/298c

**Order** (the `Ky` class in `source/core/Ky.ts`; `#run()` drives it):

1. `init` hooks run once, synchronously, in the static `Ky.create`.
2. `#runBeforeRequestHooks()` runs once, with `retryCount: 0`.
3. `#retry()` calls `#fetch()`. The first attempt returns an error response.
4. `#runAfterResponseHooks()` runs on every attempt. `#run()` then builds an `HTTPError` from the failed response and calls `#retryFromError()`.
5. `#retryFromError()` first calls `#calculateRetryDelay()`, then runs the `beforeRetry` hooks, then `#retry()` fetches again. Steps 4–5 repeat for each retry.
6. Once the retry count reaches the limit, `#calculateRetryDelay()` throws the error. `#throwProcessedError()` then runs the `beforeError` hooks. It rethrows the error unchanged if it came from a `beforeRetry` hook.

**Hooks that can replace the response:**

- `beforeRequest` can return a `Response`, which skips the fetch.
- `afterResponse` can return a new `Response`. `#runAfterResponseHooks()` swaps it in for the following hooks and for the result.
- `beforeRetry` can return a `Response`, which `#retryFromError()` returns in place of a new fetch.

`beforeError` replaces only the error, not the response.

### ky-hooks-order/6869

Order of hooks in `source/core/Ky.ts`, for a request that fails with an HTTP error until retries run out:

1. `init` runs in `Ky.create`.
2. `#runBeforeRequestHooks` runs `beforeRequest` once. It does not run again on retries.
3. `#retry` calls `#fetch`.
4. `#runAfterResponseHooks` runs `afterResponse` on every response, including non-2xx ones.
5. `#run` builds the `HTTPError`, then `#retryFromError` calls `#calculateRetryDelay`.
6. `beforeRetry` runs, and then the next attempt starts at step 3.
7. On the final attempt `#calculateRetryDelay` throws, so `beforeRetry` is skipped.
8. `#throwProcessedError` runs `beforeError` once, then throws the error.

Hooks that can replace the response:
- **`beforeRequest`:** returning a `Response` skips the network call.
- **`beforeRetry`:** returning a `Response` skips the retry fetch.
- **`afterResponse`:** returning a `Response` replaces the current one. It can also return `ky.retry()`, which forces a retry.

`beforeError` can only replace the error.

### ky-hooks-order/e692

Order, for a request that keeps failing with an HTTP error (`source/core/Ky.ts`):

1. `init` hooks run in the constructor, before anything is sent.
2. `#run()` calls `#runBeforeRequestHooks()` once, before the first attempt.
3. `#retry()` sends the request. Each response then goes through `#runAfterResponseHooks()`.
4. If the status is not ok, `#run()` builds an `HTTPError`. `#retryFromError()` then computes the delay (`#calculateRetryDelay`, which calls `shouldRetry`), waits, and runs `beforeRetry` hooks.
5. That cycle repeats from step 3 (send, `afterResponse`, `beforeRetry`) until the limit is reached.
6. When retries run out, the error goes through the `beforeError` hooks and is thrown.

`beforeRequest` does not run again on retries.

Hooks that can replace the response:
- `beforeRequest`: returning a `Response` skips the fetch.
- `afterResponse`: returning a `Response` replaces it.
- `beforeRetry`: returning a `Response` replaces it and counts as a retry.

`beforeError` replaces the error, not the response.

A response supplied by a hook is tracked in `#hookResponses`. If it is an HTTP error, `#run()` throws it without retrying.

### ky-hooks-order/6b4f

**Order for a request that keeps failing with an HTTP error:**

1. `init` hooks run once, synchronously, in the static constructor path (`Ky.create`).
2. `#run()` calls `#runBeforeRequestHooks()` once.
3. `#retry()` does the fetch.
4. `#runAfterResponseHooks()` runs on the response.
5. A non-ok status makes `#run()` build an `HTTPError`, and `#retryFromError()` runs.
6. `#retryFromError()` runs the `beforeRetry` hooks, increments the retry count and refetches.
7. Steps 4–6 repeat (`afterResponse`, then `beforeRetry`) for each attempt.
8. When retries are exhausted, `#throwProcessedError` runs the `beforeError` hooks and the final error is thrown.

`beforeRequest` runs only once, not per retry.

**Hooks that can replace the response:**

- `beforeRequest`: returning a `Response` skips the fetch.
- `afterResponse`: the returned `Response` replaces the current one.
- `beforeRetry`: returning a `Response` uses it as the result. A failing status from it throws instead of being retried, because it is tracked in `#hookResponses`.

`beforeError` can only replace the error, not the response.

I read the `#run` and `#retryFromError` sections of `core/Ky.ts` directly. The `init` and `beforeError` placement comes from grep hits and comments, not a full read.

### ky-hooks-order/fe7b

For a request that keeps failing with an HTTP error until retries run out, hooks run in this order:

1. `init` runs synchronously in `Ky.create`.
2. `beforeRequest` runs once, in `#runBeforeRequestHooks`.
3. `#fetch` sends the request.
4. `afterResponse` runs in `#runAfterResponseHooks`, before the status check. It runs on every attempt, including the last.
5. An `HTTPError` is built, then `#retryFromError` runs.
6. `beforeRetry` runs before each retry, once per retry. It doesn't run after the final failure, because the limit check throws first.
7. Steps 3–6 repeat.
8. `#throwProcessedError` runs `beforeError` once.

Hooks that can replace the response:
- `beforeRequest`: return a `Response`, which skips the fetch.
- `beforeRetry`: return a `Response`, which skips the retry. `ky.stop` ends the request with no response.
- `afterResponse`: return a `Response`. `ky.retry()` forces a retry instead.

`init` can only mutate options. `beforeError` can only replace the error.

Line numbers are in `source/core/Ky.ts`.

## ky-json-shortcut

When I call `ky.get(url).json()`, how does the Accept header get set to application/json even though `.json()` is called after `ky.get()` already returned?

Points:
1. Ky.create returns a promise object with body method shortcuts (json, text, blob...) attached
2. The shortcut sets the accept header to the method's MIME type on the pending request, unless an Accept header is already set
3. The fetch is delayed by one microtask (#run starts with await Promise.resolve()) so the synchronous shortcut call runs before the request is sent
4. If the request was already sent, the header only changes later attempts (the shortcut checks request vs original request)
5. The json shortcut then reads the body as text and parses it with parseJson or JSON.parse (with optional schema validation)

### ky-json-shortcut/7cf6

The Accept header gets set because the request hasn't been sent yet when `ky.get()` returns.

- In `Ky.create()` (`source/core/Ky.ts`), the request pipeline is built as `result`. It is an async IIFE that calls `#run()`. `ky.get()` returns this promise with the body shortcuts attached to it.
- `#run()` begins with `await Promise.resolve()`. The comment there says this is "so that body method shortcuts can set the Accept header". It defers the fetch by one microtask, so `.json()` can run before `#fetch()` is called.
- The `.json()` shortcut is defined in a loop over `getSupportedResponseTypes()`. The MIME type comes from `responseTypes[type]`. When it runs, it sets `ky.request.headers.set('accept', existing || mimeType)`, which keeps any Accept header you set yourself.
- The shortcut then awaits `result` and parses the body with `response.json()`, or with the schema if you gave one.

The header is only changed before dispatch. After dispatch, a late `.json()` call only affects later retry attempts.

### ky-json-shortcut/c74d

`ky.get(url)` returns a promise right away, but the fetch doesn't start until a microtask later. `.json()` runs in that gap and sets the header.

- **`Ky.create()`** (`source/core/Ky.ts`) wraps `#run()` in an async IIFE called `result`. It then loops over `getSupportedResponseTypes()` and attaches `.json()`, `.text()` and the rest as properties on `result`.
- **`responseTypes`** (`core/constants.ts`) maps each shortcut to its Accept value. `json` maps to `application/json`.
- **The shortcut** runs `ky.request.headers.set('accept', ky.request.headers.get('accept') || mimeType)` (`Ky.ts:385-387`). An Accept header you set yourself is kept.
- **`#run()`** starts with `await Promise.resolve()`. The comment says this delays the fetch so body shortcuts can set Accept. `#runBeforeRequestHooks()` and `#retry()` then send the request with the header already set.
- **The guard** `ky.request !== ky.#originalRequest` is true before dispatch, because `#originalRequest` is unset until a request is handed to fetch. I checked this in the source. It stops a request that was already sent from being changed.

### ky-json-shortcut/c71b

The request isn't actually sent when `ky.get()` returns, so `.json()` can still change it.

- In `source/core/Ky.ts`, the code that builds the returned promise (the static `Ky.create`) attaches a method for each response type, such as `.json()`, via the loop over `getSupportedResponseTypes()`.
- `#run()` begins with `await Promise.resolve()` (line ~591). This defers the fetch by one microtask, so `.json()` called synchronously on the returned promise runs first.
- When `.json()` is called, its closure sets the header with `ky.request.headers.set('accept', ky.request.headers.get('accept') || mimeType)`. `mimeType` comes from the `responseTypes` map (`json` maps to `application/json`).
- An Accept header you set yourself is kept, because the existing value wins.
- After dispatch, `#fetch()` replaces `ky.request` with a retry clone. A late `.json()` call then only affects later attempts, and a request that has already been sent is never changed.

### ky-json-shortcut/3e0f

The request isn't sent when `ky.get()` returns. It's deferred by one microtask, and `.json()` runs in that gap.

- **`Ky.create`** (static factory, `source/core/Ky.ts` ~360–384) returns a `ResponsePromise`. It's an async IIFE around `ky.#run()`.
- **`#run()`** begins with `await Promise.resolve()` (`Ky.ts:592`). The comment says this delays the fetch so body method shortcuts can set the Accept header. `#fetch()` and the `beforeRequest` hooks therefore run later.
- The factory then loops over **`getSupportedResponseTypes()`** (`constants.ts:58`) and attaches `json`, `text`, `blob` and so on to the promise.
- Each shortcut first runs `ky.request.headers.set('accept', ky.request.headers.get('accept') || mimeType)` (`Ky.ts:384–388`). `mimeType` comes from `responseTypes.json`, which is `application/json` (`constants.ts:46`). A header the user already set takes precedence.
- The shortcut only sets the header if `ky.request !== ky.#originalRequest`, which means the request hasn't been dispatched yet.

The subagent didn't read `#fetch()` or the `Request` construction in the constructor. So I haven't confirmed that nothing else sets a default Accept header.

### ky-json-shortcut/eda8

The fetch is deferred by one microtask, which leaves time for `.json()` to set the header before the request goes out.

- In `Ky.create()` (`source/core/Ky.ts`), the constructor builds the request and starts the pipeline, `#run()`. It then attaches body shortcuts (`.json()`, `.text()` and so on) to the returned promise. It does this in a loop over `getSupportedResponseTypes()`.
- `#run()` begins with `await Promise.resolve()`. The comment there says this is "so that body method shortcuts can set the Accept header". `ky.get(url)` returns synchronously, and your `.json()` call runs before the microtask resumes.
- The shortcut looks up `responseTypes[type]`, which is `application/json` for `json`. It then calls `ky.request.headers.set('accept', ky.request.headers.get('accept') || mimeType)`. A caller-supplied Accept header is kept.
- `#run()` then resumes, runs `#runBeforeRequestHooks()` and calls `#retry()` and `#fetch()`. The request now carries the header.
- If `.json()` is called after dispatch, the header only affects later retry attempts.

### ky-json-shortcut/3a91

The Accept header is set synchronously by `.json()`, and the real fetch is deliberately delayed until after that call.

- `ky.get` calls `createRequest`, which calls `Ky.create()` (`source/core/Ky.ts:349`). That builds the `Ky` instance and starts an async IIFE that calls `#run()`.
- `#run()` begins with `await Promise.resolve()` (`Ky.ts:591-592`). The code comment says this is so body method shortcuts can set the Accept header. It defers `#runBeforeRequestHooks()`, `#retry()` and `#fetch()` by one microtask.
- Back in `Ky.create()`, a loop over `getSupportedResponseTypes()` attaches `json`, `text` and the other shortcuts to the returned promise (`Ky.ts:381-388`). Calling `.json()` in the same tick runs `ky.request.headers.set('accept', existing || 'application/json')`. The MIME type comes from `responseTypes` in `constants.ts:46-55`, and a caller-supplied Accept header wins.
- A guard (`ky.request !== ky.#originalRequest`) stops late calls from changing a request that was already sent.

If you call `.json()` after an `await`, the fetch has already gone out and no Accept header is added.

### ky-json-shortcut/9b38

`ky.get()` doesn't send the request immediately, so `.json()` can still change the headers first.

- `createRequest` (`source/index.ts`) calls `Ky.create()` (`source/core/Ky.ts`). `Ky.create` starts the work with `ky.#run()` and returns a promise.
- `#run()` begins with `await Promise.resolve()` (`Ky.ts:592`). This delays the fetch by one microtask, so code right after `ky.get()` runs first.
- `Ky.create` then attaches shortcut methods (`json`, `text`, and so on) to that promise. The `json` shortcut checks whether the request has been sent yet (`ky.request !== ky.#originalRequest`).
- If it hasn't, it runs `ky.request.headers.set('accept', existing || 'application/json')`. A caller-supplied `Accept` header is kept.
- The mime type comes from `responseTypes.json` in `source/core/constants.ts`.
- `#fetch()` (`Ky.ts:1218`) sets `#originalRequest` when it clones and sends the request, so the header is already on it by then.

I haven't opened these files myself. This comes from the Explore agent's search, and its line numbers are approximate.

### ky-json-shortcut/c556

The header is set by the `json` shortcut that `Ky.#fetch`'s caller attaches to the promise, in `source/core/Ky.ts`.

1. `Ky.create` builds `result`, the promise from running `#run()`. It then adds a method for each type from `getSupportedResponseTypes()`, including `json`. Each method looks up its MIME type in `responseTypes`.
2. `#run()` begins with `await Promise.resolve()`. This delays the fetch by one microtask, so `.json()` can be called synchronously after `ky.get()` returns and before the request is dispatched.
3. When `.json()` runs, the shortcut checks that `ky.request !== ky.#originalRequest`, meaning the request hasn't been sent yet. It then calls `ky.request.headers.set('accept', existing || mimeType)`. A user-supplied Accept header is kept, and the MIME type is only a default.
4. The shortcut then awaits `result` and parses the body with `response.json()`.

If `.json()` is called after dispatch, the header only affects later retry attempts. It never changes a request that was already sent.

## ky-force-retry

How can an afterResponse hook force ky to retry a request, and how does a forced retry differ from a normal retry?

Points:
1. The hook returns ky.retry(...), which ky turns into a ForceRetryError
2. The run loop catches the ForceRetryError from the afterResponse hooks and goes to the retry path even for a successful response
3. A forced retry skips the retriable-method check, shouldRetry and the status code list
4. It still counts against the retry limit, and can carry a custom delay (used instead of the computed backoff)
5. It can carry a custom Request, which is applied before the beforeRetry hooks run

### ky-force-retry/1ea8

An `afterResponse` hook forces a retry by returning `ky.retry(options?)`. That function returns a branded `RetryMarker`, which `isRetryMarker` recognizes. `#runAfterResponseHooks` sees the marker and throws `ForceRetryError`, which carries the optional `delay`, `code` and replacement `request`. The main loop skips the `response.ok` check for it, so even a 200 response can be retried.

Both kinds of retry then go through `#retryFromError`. It calls `#calculateRetryDelay`, waits, runs the `beforeRetry` hooks and calls `#retry()` again.

**Differences in `#calculateRetryDelay`:**
- **Limit:** a forced retry still respects `retry.limit`. When the limit is hit, the `ForceRetryError` itself is thrown.
- **Skipped checks:** it skips `retry.methods`, `shouldRetry`, the `statusCodes` check and the `retryOnTimeout`/network-error check. A normal retry enforces all of these.
- **Delay:** a forced retry uses `customDelay`, or else the normal backoff. It never reads `Retry-After` or the rate-limit headers.
- **Request:** `error.customRequest` replaces the request, and a `beforeRetry` hook can still override it.

### ky-force-retry/05fa

An `afterResponse` hook forces a retry by returning `ky.retry()`, optionally with `{delay, code, cause, request}`. In `#runAfterResponseHooks` in `core/Ky.ts`, `isRetryMarker(modifiedResponse)` detects the marker and throws a `ForceRetryError(modifiedResponse.options)`. The error goes to `#retry()` and then `#retryFromError()`. That calls `#calculateRetryDelay()`, which is where forced and normal retries diverge.

**Differences from a normal retry:**
- **Skipped checks:** `#calculateRetryDelay()` detects the forced case with `isForceRetryError()`. It skips the `retry.methods` check, the `shouldRetry` callback, and the status and timeout eligibility checks.
- **Limit still applies:** `retry.limit` is still enforced.
- **Delay:** it uses `customDelay` if given, otherwise `#calculateDelay()`. A normal retry derives its delay from the error, for example a `Retry-After` header.
- **Custom request:** `#retryFromError()` swaps in `error.customRequest` before the `beforeRetry` hooks run.
- **Hook visibility:** `beforeRetry` and `beforeError` can identify it with `isForceRetryError()`.

### ky-force-retry/23bc

An `afterResponse` hook forces a retry by returning `ky.retry(options)`. The `retry()` function in `source/core/constants.ts` returns a `RetryMarker`. In `#runAfterResponseHooks` (`source/core/Ky.ts`), `isRetryMarker()` detects the marker and the hook throws a `ForceRetryError`. The main request loop catches it and passes it to `#retryFromError`.

**Differences from a normal retry** (decided in `#calculateRetryDelay`):
- A forced retry skips the `retry.methods` and `retry.statusCodes` checks, the 413 check, `retryOnTimeout`, and `shouldRetry`.
- Both kinds still respect `retry.limit` and share the same `retryCount`.
- A forced retry can set its own `delay`, which bypasses jitter and `backoffLimit`. A normal retry uses `#calculateDelay` instead.
- A forced retry can also pass `code`, `cause`, or a replacement `request`.
- `beforeRetry` hooks run for both. For a forced retry they receive the `ForceRetryError`, which `isForceRetryError()` identifies.

Any other error thrown from `afterResponse` is fatal and is not retried.

### ky-force-retry/ce86

An `afterResponse` hook forces a retry by returning `ky.retry()`, optionally with `{delay, request, code, cause}`. `retry()` creates a marker, and `isRetryMarker()` detects it in the `afterResponse` loop inside `Ky.ts`. That loop then throws a `ForceRetryError(modifiedResponse.options)`. The error is caught by Ky's retry handling, which calls `#calculateRetryDelay()`.

**How a forced retry differs from a normal retry:**
- **Method check:** `#calculateRetryDelay()` sees `isForceRetryError()` and returns early. It skips the `retry.methods` check, `shouldRetry`, and the `retryOnTimeout` and status checks.
- **Delay:** it uses `customDelay` if one was given. Otherwise it uses `#calculateDelay()`, the normal backoff.
- **Retry limit:** `retry.limit` still applies. When the limit is exceeded, the error goes to `beforeError`.
- **Custom request:** a `customRequest` replaces the request through `#assignRequest()` and `#withManagedSignal()` before the `beforeRetry` hooks run.
- **Observability:** `beforeRetry` hooks receive the `ForceRetryError`, which you can check with `isForceRetryError()`.
- **Trigger:** a forced retry can happen on a successful status code, because it is triggered by the response content.

### ky-force-retry/7607

An `afterResponse` hook forces a retry by returning `ky.retry()`. That is `retry()` in `core/constants.ts`, which returns a `RetryMarker`. In `#runAfterResponseHooks` (`core/Ky.ts`), the marker becomes a `ForceRetryError`. `#run` catches it and passes it to `#retryFromError`. It fires even on a 2xx response. The options are `delay`, `code`, `cause` and `request`.

**Differences from a normal retry** (a normal retry is triggered by a thrown HTTP, network or timeout error):
- `#calculateRetryDelay` still enforces `retry.limit`.
- The method, `shouldRetry`, status-code, timeout and network checks are skipped.
- A custom `delay` is used as given, so jitter, `backoffLimit` and `maxRetryAfter` don't apply. Otherwise `#calculateDelay` computes it.
- A `request` option replaces the request before the `beforeRetry` hooks run.
- `beforeRetry` hooks still run, but they receive a `ForceRetryError` as the error.
- If retries run out, the caller gets the `ForceRetryError` instead of an `HTTPError`.

### ky-force-retry/0fd3

**Forcing a retry:** an `afterResponse` hook returns `ky.retry()`, optionally with `{delay, request, cause}`. That returns a retry marker. In `#runAfterResponseHooks` (`core/Ky.ts`), `isRetryMarker(modifiedResponse)` is true, so it throws a `ForceRetryError` built from the marker's options. The retry loop catches it by checking `isForceRetryError(error)`. It then passes the error to `#calculateRetryDelay`.

**How it differs from a normal retry:** in `#calculateRetryDelay`, a `ForceRetryError` takes an early branch:
- It skips the `retry.methods` check, so POST and other non-retriable methods can be retried.
- It skips the `shouldRetry` callback.
- It skips the status-code and `Retry-After` checks and the `retryOnTimeout` rule.
- The delay is `customDelay` if one was given, otherwise `#calculateDelay` (the backoff).

It still counts against `retry.limit`. When the limit is hit, the error is thrown and goes to `beforeError`. The `beforeRetry` hooks see it, and `isForceRetryError()` detects it. A custom `request` can also replace the one used for the retry.

### ky-force-retry/5fca

An `afterResponse` hook forces a retry by returning `ky.retry()` or `ky.retry({delay, code, request})`. The returned value is a retry marker. The loop in `Ky.ts` that runs the hooks checks it with `isRetryMarker()` and throws `new ForceRetryError(options)` (`core/Ky.ts:1089`). The error then goes through `#retryFromError()`, which calls `#calculateRetryDelay()`.

A forced retry differs from a normal retry in these ways:

- **Limit:** It still respects `retry.limit`. Once the limit is hit, the `ForceRetryError` is thrown.
- **Skipped checks:** `#calculateRetryDelay()` skips the HTTP method check, the status-code check and the `shouldRetry` callback. A normal retry runs all of these.
- **Delay:** It uses `customDelay` if one was given. Otherwise it falls back to `#calculateDelay()`, the normal backoff.
- **Replacement request:** `#retryFromError()` swaps in `error.customRequest` (via `#assignRequest()`) before the `beforeRetry` hooks run. A normal retry has no such option.
- **Observability:** `beforeRetry` hooks see a `ForceRetryError`, which `isForceRetryError()` detects.

### ky-force-retry/0c6f

An `afterResponse` hook forces a retry by returning `ky.retry(options?)` (`source/core/constants.ts`). That returns a `RetryMarker`. The options can include `delay`, `code`, `cause` and `request`.

`#runAfterResponseHooks` in `source/core/Ky.ts` sees the marker and throws a `ForceRetryError`. The `#run` loop catches it and calls `#retryFromError`. That function calls `#calculateRetryDelay`, runs the `beforeRetry` hooks, increments `#retryCount`, and calls `#retry()`.

**Forced vs normal retry**, in `#calculateRetryDelay`:
- **Limit:** a forced retry uses the same `retry.limit` budget. Once the limit is hit, the `ForceRetryError` is thrown.
- **Skipped checks:** a forced retry skips the method list, `shouldRetry`, status codes, `retryOnTimeout` and network-error checks. It can retry POST, or a 2xx response.
- **Delay:** a forced retry uses `customDelay` as given, with no jitter or `backoffLimit`. It ignores `Retry-After` headers. Otherwise `#calculateDelay` computes the delay.
- **Request:** a forced retry can supply a replacement `request`.

The numbers above come from the Explore agent's report. I haven't opened the source myself.

## requests-get-flow

Trace a call to `requests.get(url)` down to the network: which functions and classes run, in order?

Points:
1. requests.get calls api.request, which opens a Session in a with block and calls session.request
2. Session.request builds a Request and calls prepare_request (merging session headers, cookies, auth and hooks into a PreparedRequest)
3. Session.request calls merge_environment_settings (proxies, verify, cert from the environment) and then Session.send
4. Session.send picks an adapter with get_adapter and calls HTTPAdapter.send, which uses a urllib3 connection pool (urlopen) and build_response
5. After the adapter, send dispatches the response hooks, extracts cookies into the session jar and follows redirects with resolve_redirects

### requests-get-flow/cb08

Call order for `requests.get(url)`, from my knowledge of the library (I didn't open the repo for this):

1. `requests.api.get` calls `requests.api.request`.
2. `request` opens `sessions.Session()` as a context manager and calls `Session.request`.
3. `Session.request` builds a `Request` and calls `Session.prepare_request`. That merges cookies and auth and runs `PreparedRequest.prepare`, which calls `prepare_method`, `prepare_url`, `prepare_headers`, `prepare_cookies`, `prepare_body` and `prepare_auth`.
4. `Session.request` then calls `Session.merge_environment_settings` (proxies and verify) and `Session.send`.
5. `Session.send` picks an adapter with `get_adapter`, which returns an `HTTPAdapter`. It calls `HTTPAdapter.send`, then runs hooks and `resolve_redirects`.
6. `HTTPAdapter.send` calls `get_connection_with_tls_context` (older versions: `get_connection`), `cert_verify` and `request_url`. It then calls urllib3's `conn.urlopen`.
7. In urllib3, `PoolManager`/`HTTPConnectionPool.urlopen` calls `_make_request`, which calls `HTTPConnection.request` and then `http.client`. That reaches `socket.sendall`.
8. `HTTPAdapter.build_response` wraps the result in a `Response`.

### requests-get-flow/9b35

Call chain for `requests.get(url)`, in order:

1. `api.get` calls `api.request`, which opens `sessions.Session()` as a context manager and calls `session.request`.
2. `Session.request` builds a `models.Request` and calls `Session.prepare_request`. That merges the session settings, cookies and auth, then calls `PreparedRequest.prepare`. `prepare` runs `prepare_method`, `prepare_url`, `prepare_headers`, `prepare_cookies`, `prepare_body` and `prepare_auth`.
3. `Session.request` then calls `merge_environment_settings` for proxies and verify, and `Session.send`.
4. `Session.send` calls `get_adapter`, which returns an `adapters.HTTPAdapter`. It then calls `HTTPAdapter.send`.
5. `HTTPAdapter.send` calls `get_connection_with_tls_context`, which picks a pool through `poolmanager.PoolManager` or a proxy manager. It then calls `cert_verify` and `request_url`.
6. `HTTPAdapter.send` calls `conn.urlopen`, which is urllib3's `HTTPConnectionPool.urlopen`. Below that are `_make_request`, `HTTPConnection.request` and `getresponse`, then `http.client` and the socket.
7. `HTTPAdapter.build_response` returns a `Response`.
8. Back in `Session.send`, the hooks run via `dispatch_hook`. `resolve_redirects` handles any redirects, and the content is read unless `stream=True`.

### requests-get-flow/c072

Call path for `requests.get(url)`:

1. `api.get` (api.py:74) calls `api.request` (api.py:24). That opens a `Session()` context manager, which runs `Session.__init__` and mounts the default `HTTPAdapter`s.
2. `Session.request` (sessions.py:557) builds a `Request`. It then calls `prepare_request`, which runs `PreparedRequest.prepare`. That runs `prepare_method`, `prepare_url`, `prepare_headers`, `prepare_cookies`, `prepare_body`, `prepare_auth` and `prepare_hooks`.
3. `merge_environment_settings` resolves proxies and the CA bundle.
4. `Session.send` (sessions.py:752) calls `get_adapter`, which matches the URL prefix to a mounted adapter.
5. `HTTPAdapter.send` (adapters.py:634) calls `get_connection_with_tls_context`, which returns a urllib3 `PoolManager` connection pool. It also calls `cert_verify`, `request_url` and `add_headers`.
6. `conn.urlopen` (adapters.py:696) hands off to urllib3, which opens the TCP/TLS socket, sends the request and reads the headers.
7. `build_response` (adapters.py:365) wraps the urllib3 response in a `Response`.
8. Back in `Session.send`, response hooks and cookie extraction run, and `resolve_redirects` follows any redirects (each hop goes through `send` again). The body is read unless `stream=True`.

### requests-get-flow/dce0

Order of calls for `requests.get(url)`:

1. `api.get()` calls `api.request("get", url, ...)`.
2. `api.request()` opens `sessions.Session()` as a context manager and calls `session.request()`.
3. `Session.request()` builds a `models.Request`. `Session.prepare_request()` merges in the session settings (cookies, auth, headers) and calls `PreparedRequest.prepare()`. That runs `prepare_method`, `prepare_url`, `prepare_headers`, `prepare_cookies`, `prepare_body` and `prepare_auth`.
4. `Session.merge_environment_settings()` picks up proxies and `verify` from the environment.
5. `Session.send()` calls `get_adapter()`, which returns an `adapters.HTTPAdapter`. It then calls `adapter.send()`, and afterwards runs the response hooks and `resolve_redirects()` if needed.
6. `HTTPAdapter.send()` calls `get_connection_with_tls_context()` (older versions: `get_connection()`), then `cert_verify()` and `request_url()`. This yields a urllib3 `PoolManager` or `ProxyManager` connection pool.
7. `conn.urlopen()` runs urllib3's `HTTPConnectionPool.urlopen`, then `_make_request`, then `HTTPConnection.request`, which opens the socket and sends the request.
8. `HTTPAdapter.build_response()` wraps the urllib3 response in a `models.Response`.

### requests-get-flow/3fac

Call order for `requests.get(url)`:

1. `requests.api.get` calls `requests.api.request`, which opens `sessions.Session()` as a context manager and calls `session.request`.
2. `Session.request` builds a `Request` and calls `Session.prepare_request`. That merges the settings and runs `PreparedRequest.prepare`, which calls `prepare_method`, `prepare_url`, `prepare_headers`, `prepare_cookies`, `prepare_body` and `prepare_auth`.
3. It then calls `Session.merge_environment_settings` (proxies, verify) and `Session.send`.
4. `Session.send` calls `get_adapter`, which returns an `HTTPAdapter`, then `HTTPAdapter.send`.
5. `HTTPAdapter.send` calls `get_connection_with_tls_context` (older versions: `get_connection`), `cert_verify` and `request_url`. It then calls `conn.urlopen`, which is urllib3's `HTTPConnectionPool.urlopen`. That runs `_make_request`, which uses `HTTPConnection.connect` to open the socket and TLS, then `getresponse`.
6. `HTTPAdapter.build_response` creates the `Response`.
7. Back in `Session.send`, `dispatch_hook` runs, cookies are extracted, and `resolve_redirects` handles any redirects. The body is read unless `stream=True`.

### requests-get-flow/04b5

`requests.get(url)` runs these steps, in order:

1. `api.get` (`api.py:74`) calls `api.request`.
2. `api.request` opens a throwaway `Session()` (`Session.__init__` mounts the default `HTTPAdapter`s). It then calls `Session.request`.
3. `Session.request` (`sessions.py:557`) builds a `Request`. It calls `prepare_request`, which calls `PreparedRequest.prepare` (`models.py:424`). That runs `prepare_method`, `prepare_url`, `prepare_headers`, `prepare_cookies`, `prepare_body`, `prepare_auth` and `prepare_hooks`.
4. `Session.request` then calls `merge_environment_settings` and `Session.send` (`sessions.py:752`).
5. `Session.send` calls `get_adapter`, which picks the `HTTPAdapter` by URL prefix. It then calls `HTTPAdapter.send` (`adapters.py:634`).
6. `HTTPAdapter.send` calls:
   - `get_connection_with_tls_context`, which returns a urllib3 pool from the `PoolManager`, or from a proxy manager if a proxy applies.
   - `cert_verify`.
   - `conn.urlopen(...)` (`adapters.py:696`). From here urllib3 takes over: `_make_request`, DNS lookup, `socket.create_connection`, TLS handshake, send.
7. `HTTPAdapter.build_response` (`adapters.py:365`) wraps the urllib3 response in a `Response`.
8. Back in `Session.send`:
   - `dispatch_hook("response")` fires.
   - Cookies are extracted into the session jar.
   - `resolve_redirects` follows any redirects, re-entering `send` for each hop.
   - The body is read if `stream` is false.

### requests-get-flow/9cb3

`requests.get(url)` runs these steps in order:

1. `api.get` (api.py:74) calls `api.request`, which opens `with Session()` and calls `Session.request`.
2. `Session.request` (sessions.py:557) builds a `Request` and calls `prepare_request`. That creates a `PreparedRequest` and calls `PreparedRequest.prepare` (models.py:424). `prepare` runs `prepare_method`, `prepare_url`, `prepare_headers`, `prepare_cookies`, `prepare_body`, `prepare_auth` and `prepare_hooks`.
3. `Session.request` then calls `merge_environment_settings` and `Session.send` (sessions.py:752).
4. `Session.send` calls `get_adapter`, which returns an `HTTPAdapter`, and then `HTTPAdapter.send` (adapters.py:634).
5. `HTTPAdapter.send` calls `get_connection_with_tls_context`, which gets a urllib3 pool from `poolmanager.connection_from_host`. It then calls `cert_verify`, `request_url` and `add_headers`, followed by `conn.urlopen(...)` (adapters.py:696), the handoff to urllib3.
6. `build_response` wraps the urllib3 response in a `Response`.
7. Back in `Session.send`, it runs `dispatch_hook` and `extract_cookies_to_jar`. It calls `resolve_redirects` if there are redirects.

The urllib3 part below is from memory, because the agent couldn't open the installed source, so I haven't checked it against the installed version. `urlopen` calls `_get_conn`, `_new_conn` and `_make_request`. Those lead to `connection.create_connection`, then `socket.getaddrinfo` and `socket.connect`. HTTPS adds a TLS wrap. `http.client` then writes the request to the socket.

### requests-get-flow/03b9

Call path for `requests.get(url)`, in order (files under `src/requests/`):

1. `api.get` (`api.py:87`) calls `api.request`.
2. `api.request` (`api.py:70`) opens `with sessions.Session()` and calls `session.request`.
3. `Session.request` (`sessions.py:557`) builds a `Request`, then calls `Session.prepare_request` (`:511`). That produces a `PreparedRequest`, via `PreparedRequest.prepare` in `models.py`. It then calls `merge_environment_settings` and `Session.send`.
4. `Session.send` (`sessions.py:752`) calls `Session.get_adapter` (`:870`), which returns the `HTTPAdapter` mounted for the URL prefix.
5. `HTTPAdapter.send` (`adapters.py:634`) runs these steps:
   - `get_connection_with_tls_context` (`:455`) picks `proxy_manager_for` or `poolmanager.connection_from_host`.
   - `cert_verify`, `request_url` and `add_headers` prepare the request.
   - urllib3's `conn.urlopen` (`:696`) opens the socket and sends the request. I didn't read urllib3's source.
   - `build_response` (`:365`) wraps the result in a `Response`.
6. Back in `Session.send`, the response hook is dispatched via `dispatch_hook` and cookies are extracted. `resolve_redirects` (`:186`) follows any redirects, each hop calling `send` again.

## requests-redirects

How does a requests Session follow redirects, and when does it drop the Authorization header on a redirect?

Points:
1. Session.send calls resolve_redirects, a generator, when allow_redirects is true
2. Each hop reads the Location header (get_redirect_target), consumes and closes the old response, and stops with TooManyRedirects at max_redirects (30)
3. rebuild_method changes the method (303 and 302 to GET unless HEAD, 301 only for POST)
4. rebuild_auth removes the Authorization header when should_strip_auth is true, which happens when the hostname changes
5. An http to https upgrade on the same host with default ports keeps the header; other scheme or port changes strip it

### requests-redirects/0151

**Following redirects:** `Session.send` calls the adapter. If `allow_redirects` is true, it iterates `resolve_redirects`. That generator loops on `get_redirect_target`, which reads the `Location` header. Each pass copies the request and resolves the URL. It calls `rebuild_method` (303 becomes GET, and 302 or 301 POST becomes GET). It drops body headers unless the status is 307 or 308. It rebuilds cookies, then calls `rebuild_proxies` and `rebuild_auth`. It re-sends with `allow_redirects=False`. It raises `TooManyRedirects` past `max_redirects`.

**Dropping Authorization:** `rebuild_auth` deletes the header when `should_strip_auth(old_url, new_url)` returns True. That happens when:
- the hostname changes;
- the port or scheme changes, except an http→https upgrade on default ports, which is kept;
- the scheme is the same and both ports are default or unspecified, which is also kept.

With `trust_env` set, `rebuild_auth` then re-applies `.netrc` credentials for the new URL.

### requests-redirects/361c

`Session.send` calls `resolve_redirects` when `allow_redirects=True`. That generator loops as follows:

1. `get_redirect_target` reads the `Location` header, but only if `resp.is_redirect`.
2. It copies the request, enforces `max_redirects` (raising `TooManyRedirects`), and builds the new URL. That covers `//` scheme-relative and relative `Location` values, fragment carry-over, and `requote_uri`.
3. `rebuild_method` changes the method as needed. For statuses other than 307 and 308, it drops the body and the Content-Length, Content-Type and Transfer-Encoding headers.
4. It drops the `Cookie` header, then rebuilds cookies.
5. `rebuild_proxies` and `rebuild_auth` run, and the body is rewound if needed.
6. It re-sends with `allow_redirects=False`.

`rebuild_auth` deletes `Authorization` when `should_strip_auth(old_url, new_url)` returns True. That happens when the hostname changes, or when the scheme or port changes. Two cases are exempt:
- An `http` to `https` upgrade on the default ports (80 or unspecified, to 443 or unspecified).
- The same scheme with default or unspecified ports on both sides.

After stripping, `rebuild_auth` re-applies credentials from `.netrc` for the new URL via `get_netrc_auth`, if `trust_env` is set.

### requests-redirects/d62a

`Session.send` calls `SessionRedirectMixin.resolve_redirects`, a generator. It runs when `allow_redirects` is true.

**Redirect loop**
- `get_redirect_target` reads the `Location` header of a redirect response.
- Each loop iteration copies the previous request and resolves a relative URL against the old one.
- It calls `rebuild_method`, which switches to GET on 303, and on 302 or 301 for non-GET/HEAD requests.
- It calls `rebuild_proxies` and `rebuild_auth`, then sends the new request.
- It raises `TooManyRedirects` once `max_redirects` is exceeded.

**When Authorization is dropped**
- `rebuild_auth` deletes the `Authorization` header when `should_strip_auth(old_url, new_url)` returns True.
- `should_strip_auth` returns True if the hostname changed.
- It also returns True if the scheme or port changed.
- The exceptions are an http→https upgrade on the standard ports (80 or none, then 443 or none) and a same-scheme change between an explicit default port and no port. Those keep the header.
- After stripping, `rebuild_auth` re-applies `.netrc` credentials if `trust_env` is enabled.

### requests-redirects/f14b

**Redirect flow.** `Session.send` calls the adapter. If `allow_redirects` is true, it runs `SessionRedirectMixin.resolve_redirects`, which loops while `get_redirect_target` returns a `Location`. Each hop does the following:
- Copies the request and records `history`.
- Raises `TooManyRedirects` once `max_redirects` (30) is reached.
- Calls `rebuild_method`, which turns 303 and 302 into GET and 301 POST into GET, while 307 and 308 keep the method.
- Drops the body and the `Content-*` and `Transfer-Encoding` headers unless the status is 307 or 308.
- Calls `rebuild_proxies` and `rebuild_auth`.
- Resends via `send(..., allow_redirects=False)`.

**Authorization.** `rebuild_auth` deletes the `Authorization` header only when `should_strip_auth(old_url, new_url)` returns True. That happens when:
- The hostname changes.
- The scheme or port changes, with two exceptions that keep the header: an http→https upgrade on the default ports, and no change in scheme with both ports at the scheme's default.

`rebuild_auth` then re-applies netrc credentials if `trust_env` is on.

### requests-redirects/7c7b

**Redirect following:** `Session.send` (sessions.py) sends the request. If `allow_redirects` is true, it consumes the `resolve_redirects` generator. That generator loops while `get_redirect_target` returns a `Location` (the response's `is_redirect` is true). Each iteration:
- It enforces `max_redirects` (raising `TooManyRedirects`).
- It resolves the URL.
- It calls `rebuild_method`: 303 and 302 become GET, 301 POST becomes GET, and 307/308 keep the method.
- It drops the body headers (except on 307/308).
- It calls `rebuild_proxies` and `rebuild_auth`.
- It re-sends with `allow_redirects=False`.

**Authorization dropping:** `rebuild_auth` deletes `Authorization` when `should_strip_auth(old_url, new_url)` is true. `should_strip_auth` works as follows:
- It strips if the hostname changes.
- It keeps the header for an http→https upgrade on default ports (80 to 443).
- It keeps the header if the scheme is the same and both ports are default.
- Otherwise it strips on any scheme or port change.

If `trust_env` is on, `rebuild_auth` then re-applies netrc credentials for the new URL via `get_netrc_auth`.

### requests-redirects/a318

**Following redirects:** `Session.send` calls `resolve_redirects` (a generator in `sessions.py`) when `allow_redirects` is true. Each loop iteration does the following:

- `get_redirect_target` reads the `Location` header of a redirect response.
- The loop copies the previous request.
- `rebuild_method` adjusts the method, for example 303 turns it into GET.
- `rebuild_proxies` re-evaluates the proxy settings.
- `rebuild_auth` is called on the new request.
- The loop sends the new request and repeats until `max_redirects` is exceeded, which raises `TooManyRedirects`.

**Dropping Authorization:** `rebuild_auth` deletes the `Authorization` header when `should_strip_auth(old_url, new_url)` returns True. That happens when:

- The hostname changes.
- The scheme or port changes.

It returns False, so the header is kept, in two cases:

- An http→https upgrade on the standard ports (80 to 443).
- The same scheme with default or unspecified ports on both URLs.

After stripping, `rebuild_auth` may re-apply credentials from `.netrc` for the new URL via `get_netrc_auth`, but only if `trust_env` is on.

### requests-redirects/2365

**Following redirects:** `Session.send` (in `sessions.py`) sends the request. If `allow_redirects` is true, it calls `SessionRedirectMixin.resolve_redirects`. That generator loops over hops. `get_redirect_target` reads the `Location` header, and `max_redirects` is enforced. Each hop then:
- calls `rebuild_method`, which maps 303 and 302 to GET and 301 POST to GET, while 307 and 308 keep the method;
- strips the body and content headers when the method changes;
- resets cookies;
- calls `rebuild_proxies` and `rebuild_auth`;
- re-sends the request.

**Dropping Authorization:** `rebuild_auth` deletes the header when `should_strip_auth(old_url, new_url)` returns True. That happens when:
- the hostname differs, including subdomains;
- the port differs;
- the scheme changes, except for an http→https upgrade on default ports.

Same-host redirects between default ports keep the header. After stripping, if `trust_env` is on, `rebuild_auth` calls `get_netrc_auth` for the new URL, which can add credentials for the new host.

### requests-redirects/b26f

`Session.send` calls `SessionRedirectMixin.resolve_redirects`. That generator loops while `get_redirect_target` returns a `Location` URL. It stops with `TooManyRedirects` once `max_redirects` is exceeded.

Each iteration does the following:
- It copies the previous request and resolves the URL, whether relative or absolute.
- It calls `rebuild_method`, which changes the method for 303, 302 and 301 responses.
- It drops the body headers when the method changes.
- It calls `rebuild_proxies`.
- It calls `rebuild_auth`.
- It sends the new request through `Session.send`.

`rebuild_auth` deletes the `Authorization` header when `should_strip_auth(old_url, new_url)` returns True. If no `.netrc` credentials apply, it also leaves the header off after stripping.

`should_strip_auth` returns True when:
- the hostname changes, or
- the port or scheme changes.

It returns False for:
- an http→https upgrade on the standard ports (80 or unspecified → 443 or unspecified), or
- the same scheme with default or unspecified ports on both sides.

## requests-adapters

How does requests pick which transport adapter handles a URL, and how do you plug in your own adapter?

Points:
1. A new Session mounts an HTTPAdapter for both https:// and http://
2. get_adapter loops over the mounted adapters and returns the first whose prefix matches the start of the URL, case-insensitively
3. mount keeps the adapters ordered by prefix length, longest first, so the most specific prefix wins
4. If no prefix matches, get_adapter raises InvalidSchema
5. To plug in your own: subclass BaseAdapter (or HTTPAdapter), implement send and close, and call session.mount(prefix, adapter)

### requests-adapters/ef3d

**Selection:** `Session.send()` (sessions.py:778) calls `Session.get_adapter(url)` (sessions.py:870). That function walks the `self.adapters` OrderedDict. It returns the first adapter whose prefix matches the URL, compared case-insensitively with `startswith`. If nothing matches, it raises `InvalidSchema`.

**Ordering:** `Session.__init__` sets up `self.adapters` and mounts the default `HTTPAdapter` for `"https://"` and `"http://"`. `Session.mount(prefix, adapter)` (sessions.py:888) inserts the adapter. It then moves every shorter prefix to the end of the dict. This keeps the longest prefixes first, so the most specific prefix wins.

**Plugging in your own:** Subclass `requests.adapters.BaseAdapter` or `HTTPAdapter`, and override `send()` and `close()`. Then mount it:

```python
s = requests.Session()
s.mount("https://api.example.com/", MyAdapter())
```

Requests to URLs under that prefix then go through `MyAdapter`.

### requests-adapters/7301

**Selection:** `Session.send()` (sessions.py:778) calls `Session.get_adapter(url)` (line 870). That function walks the `self.adapters` OrderedDict and returns the first adapter whose prefix matches the URL, compared case-insensitively with `url.lower().startswith(prefix.lower())`. If nothing matches, it raises `InvalidSchema`.

**Ordering:** `Session.__init__` (lines 502–503) calls `mount()` to register a default `HTTPAdapter` for `"https://"` and `"http://"`. `Session.mount(prefix, adapter)` (line 888) stores the adapter, then moves every shorter prefix to the end of the dict. This keeps the dict sorted longest-prefix-first, so the most specific prefix wins.

**Plugging in your own:** Subclass `requests.adapters.BaseAdapter`, or `HTTPAdapter` for HTTP-based transports. Implement `send()` and `close()`, then register it:

```python
s = requests.Session()
s.mount("https://api.example.com/", MyAdapter())
```

Because that prefix is longer than `"https://"`, it takes precedence for matching URLs.

### requests-adapters/b6a8

**Selection:** `Session.send` (`sessions.py`) calls `Session.get_adapter(url)`. That function walks `Session.adapters`, an ordered dict. It returns the first adapter whose prefix matches the start of the URL, compared case-insensitively. If nothing matches, it raises `InvalidSchema`.

`Session.__init__` mounts a default `HTTPAdapter` for `"https://"` and `"http://"`.

**Ordering:** `Session.mount(prefix, adapter)` stores the adapter, then moves every shorter prefix to the end of the dict. Longer, more specific prefixes therefore win, so `"https://api.example.com/"` beats `"https://"`.

**Plugging in your own:** subclass `requests.adapters.BaseAdapter`, or `HTTPAdapter`, and override `send()` and `close()`. Then register it:

```python
s = requests.Session()
s.mount("https://api.example.com/", MyAdapter())
```

The prefix must include the scheme. Common uses are retries via `HTTPAdapter(max_retries=...)`, custom TLS settings, and non-HTTP schemes.

### requests-adapters/472b

**Selection:** `Session.send()` (`sessions.py`) calls `Session.get_adapter(url)`. That method walks `self.adapters`, an `OrderedDict` of prefix to adapter. It returns the first adapter whose prefix matches the URL, compared case-insensitively with `url.lower().startswith(prefix.lower())`. If nothing matches, it raises `InvalidSchema`.

**Ordering:** `Session.__init__` calls `mount()` to register a default `HTTPAdapter` for `https://` and for `http://`. `Session.mount(prefix, adapter)` stores the adapter, then moves any shorter existing prefixes to the end. The dict stays ordered longest prefix first, so the most specific prefix wins.

**Plugging in your own:**
1. Subclass `BaseAdapter` (implement `send` and `close`) or subclass `HTTPAdapter`.
2. Call `session.mount("https://api.example.com/", MyAdapter())`.

Requests whose URL starts with that prefix then go through your adapter. `Session.close()` closes every mounted adapter.

### requests-adapters/17c1

**Selection:** `Session.send` (sessions.py:778) calls `Session.get_adapter(url)`. That method walks `self.adapters`, an `OrderedDict`, and returns the first adapter whose prefix matches the URL. Matching uses `url.lower().startswith(prefix.lower())`. If nothing matches, it raises `InvalidSchema`. `Session.__init__` mounts a default `HTTPAdapter()` for `"https://"` and `"http://"`.

**Ordering:** `Session.mount(prefix, adapter)` stores the adapter and then moves every shorter prefix to the end of the dict. The dict therefore stays sorted longest-prefix-first, so the most specific prefix wins.

**Plugging in your own:**
1. Subclass `requests.adapters.BaseAdapter`, or `HTTPAdapter`. Implement `send(request, **kwargs)`, which must return a `Response`, and `close()`.
2. Mount it on a session: `s = requests.Session(); s.mount("https://api.example.com", MyAdapter())`.

Redirects go back through `send`, so the adapter is chosen again for each hop.

### requests-adapters/679e

**Selection:** `Session.get_adapter(url)` in `src/requests/sessions.py` iterates over `self.adapters`, an `OrderedDict` mapping URL prefixes to adapters. It returns the first adapter whose prefix matches the start of the URL, compared case-insensitively. If none match, it raises `InvalidSchema`. `Session.send()` calls `get_adapter` to choose the adapter, then calls that adapter's `send()`.

**Ordering:** `Session.__init__` mounts `HTTPAdapter()` for `"https://"` and `"http://"`. `Session.mount(prefix, adapter)` registers an adapter and then moves every shorter prefix after the new one. This keeps the dict ordered longest prefix first, so the most specific prefix wins.

**Plugging in your own:** Subclass `requests.adapters.BaseAdapter` (or `HTTPAdapter`). Implement `send()` and `close()`, then register it:

```python
s = requests.Session()
s.mount("https://api.example.com/", MyAdapter())
```

`Session.close()` calls `close()` on every mounted adapter.

### requests-adapters/fb98

**Selection:** `Session.send` (sessions.py:778) calls `Session.get_adapter(url)`. That method scans `self.adapters`, an `OrderedDict` of prefix to adapter. It returns the first adapter whose prefix matches the URL, compared case-insensitively with `startswith`. If none match, it raises `InvalidSchema`.

`Session.__init__` mounts two separate `HTTPAdapter` instances, one for `https://` and one for `http://`.

**Longest prefix wins:** `Session.mount(prefix, adapter)` stores the adapter. It then moves every shorter prefix to the end of the dict. This keeps the dict ordered longest-first, so `https://example.com/api` is checked before `https://`.

**Custom adapter:**
1. Subclass `BaseAdapter` (adapters.py:122), or `HTTPAdapter`.
2. Implement `send(request, stream, timeout, verify, cert, proxies)`, which must return a `Response`. Also implement `close()`.
3. Register it with `session.mount("myscheme://", MyAdapter())`.

`Session.close` calls `close()` on every mounted adapter.

### requests-adapters/7c58

**Selection:** `Session.send()` (`sessions.py:778`) calls `Session.get_adapter(url)`. That method loops over `self.adapters`, an `OrderedDict` that `Session.__init__` fills with `"https://"` and `"http://"` mapped to `HTTPAdapter`. It returns the first adapter whose prefix matches the URL. The match is a case-insensitive `startswith`. If nothing matches, it raises `InvalidSchema`.

**Ordering:** `Session.mount(prefix, adapter)` adds the entry and then re-inserts every shorter prefix after it. This keeps the list sorted with the longest prefix first, so `https://api.example.com/` is checked before `https://`.

**Plugging in your own:**
1. Subclass `requests.adapters.BaseAdapter`, or subclass `HTTPAdapter`.
2. Implement `send()` and `close()`.
3. Register it with `session.mount("myscheme://", MyAdapter())`.

`Session.close()` calls `close()` on every mounted adapter.

## requests-env-settings

Where do proxy and CA bundle settings from environment variables get merged into a requests call, and which source wins when they conflict?

Points:
1. Session.request calls merge_environment_settings after preparing the request, and it only reads the environment when trust_env is true
2. Environment proxies come from get_environ_proxies (respecting no_proxy) and are added with setdefault, so proxies passed explicitly win
3. REQUESTS_CA_BUNDLE, then CURL_CA_BUNDLE, replace verify when verify is True or None
4. Then merge_setting combines request-level and session-level values, with the request-level value winning
5. Consequence: session.verify = False can be overridden by a CA bundle from the environment, because the request-level verify is None and gets the env value first

### requests-env-settings/e636

The merge happens in `Session.merge_environment_settings` (`src/requests/sessions.py:831`). `Session.request` calls it when preparing each request.

**Proxies**
- When `trust_env` is true, `get_environ_proxies` (`utils.py`) reads the environment proxies. It honors `no_proxy` through `should_bypass_proxies`.
- Those are added to the per-request `proxies` with `setdefault`, so **per-request `proxies` beat the environment**.
- `merge_setting(proxies, self.proxies)` then merges in the session's proxies. Per-request values win over `session.proxies`.
- Because the environment is folded into the per-request dict first, **environment proxies beat `session.proxies`**.
- This only happens if `proxies is not None`.

**CA bundle**
- If `verify` is `True` or `None`, `REQUESTS_CA_BUNDLE` is used, then `CURL_CA_BUNDLE`, then the original value.
- An explicit `verify=False` or `verify="path"` on the request wins over both.
- The environment value also overrides `session.verify`, because `merge_setting(verify, self.verify)` runs afterward and the environment value is already set.

**Redirects**
- `Session.rebuild_proxies` (`sessions.py:334`) re-applies the proxy settings during redirects.

### requests-env-settings/36e4

Environment settings are merged in `Session.merge_environment_settings` (sessions.py:831). `Session.request` calls it, and it uses `merge_setting` to combine request and session values.

**Proxies**
- If `trust_env` is on, `get_environ_proxies` and `should_bypass_proxies` (utils.py) supply the environment values.
- Those values are added with `setdefault`, so they only fill schemes the per-request `proxies` left unset.
- Request values then override session values, but the environment has already been folded into the request dict. So `HTTP_PROXY` beats `session.proxies` for unset keys.
- A `no_proxy` entry in the request dict beats the `NO_PROXY` variable.
- `resolve_proxies` and `rebuild_proxies` redo this on `Session.send` and on redirects.

**CA bundle**
- If `trust_env` is on and `verify` is `True` or `None`, `verify` becomes `REQUESTS_CA_BUNDLE`, then `CURL_CA_BUNDLE`.
- This beats `session.verify`.
- An explicit `verify=False` or a path string is never overridden.

With `trust_env=False`, the environment is ignored.

### requests-env-settings/b955

The merge happens in `Session.merge_environment_settings` (`src/requests/sessions.py:831`). `Session.request` calls it when it prepares the request.

**Proxies**
- When `trust_env` is true, `get_environ_proxies` (`utils.py`, which uses `should_bypass_proxies` for `no_proxy`) reads the environment proxies.
- They are added with `proxies.setdefault`, so a proxy passed in the call beats the environment.
- `merge_setting(proxies, self.proxies)` then combines the result with the session-level proxies. Request-level values win over session values.
- Precedence: request `proxies=` > environment > `Session.proxies`.
- The environment proxies are only merged when the request's `proxies` is not `None`. I didn't check how `Session.request` calls this function.

**CA bundle**
- If `verify` is `True` or `None`, it becomes `REQUESTS_CA_BUNDLE`, then `CURL_CA_BUNDLE`, falling back to the original value.
- `merge_setting(verify, self.verify)` is then applied.
- An explicit `verify=False` or a path in the call beats the environment.
- Because the environment value replaces `None` or `True` before the session merge, it also beats `Session.verify`.

### requests-env-settings/fc99

Environment settings are merged in `Session.request` (`src/requests/sessions.py`), which calls `Session.merge_environment_settings`. That function runs only when `trust_env` is true.

**Proxies**
- `get_environ_proxies` (`utils.py`) reads `HTTP_PROXY`, `HTTPS_PROXY` and `ALL_PROXY`. `should_bypass_proxies` applies `NO_PROXY`.
- The environment values are added with `setdefault`, so explicit `proxies=` wins. Environment proxies then beat `Session.proxies`.
- A `no_proxy` key in the explicit dict overrides the environment's `NO_PROXY`.

**CA bundle**
- If `verify` is `True` or `None`, `REQUESTS_CA_BUNDLE` and then `CURL_CA_BUNDLE` replace it.
- An explicit `verify=False` or a path string wins over the environment.
- The environment variables also override `Session.verify`, even `Session.verify=False`.

After that, `merge_setting` combines the request values with the session attributes, and the request values win. `resolve_proxies` and `rebuild_proxies` apply the same logic on redirects. `HTTPAdapter.cert_verify` then applies the final `verify` value.

### requests-env-settings/b074

**Where the merge happens:** `Session.request` (sessions.py) calls `Session.merge_environment_settings` (~lines 831–868). That function only reads the environment when `trust_env` is true. It then finishes with `merge_setting`, which combines the request values with the session values.

**Proxies:** `get_environ_proxies` and `should_bypass_proxies` (utils.py) supply the env proxies and apply NO_PROXY. These are added with `setdefault`, so precedence is:
1. The per-request `proxies=` argument.
2. The env proxies (`HTTP_PROXY` etc.), unless NO_PROXY matches the URL.
3. `Session.proxies`.

Env proxies therefore override `session.proxies`. `select_proxy` picks the entry for the URL. On redirects, `rebuild_proxies` calls `resolve_proxies`, which also uses `setdefault`.

**CA bundle:** the env lookup only runs if the request's `verify` is `True` or `None`. Precedence is:
1. An explicit `verify=False` or a path on the request.
2. `REQUESTS_CA_BUNDLE`.
3. `CURL_CA_BUNDLE`.
4. `Session.verify`.
5. The certifi default.

`HTTPAdapter.cert_verify` only consumes the final value and does no env lookup.

### requests-env-settings/20d5

The merge happens in `Session.merge_environment_settings` (`src/requests/sessions.py:831`). `Session.request` calls it after building the `Request`, and it only does the environment work when `trust_env` is true.

**Proxies**
- `get_environ_proxies` (`utils.py`) reads the `HTTP_PROXY`, `HTTPS_PROXY` and `NO_PROXY` environment variables. It uses `should_bypass_proxies` to apply `no_proxy`.
- Those values are added to the per-request `proxies` with `setdefault`, so a proxy passed to the call beats the environment.
- `merge_setting(proxies, self.proxies)` then merges in the session's proxies, with request-level values taking precedence over session-level ones.
- Because only `setdefault` is used, an environment proxy fills a gap in the request dict. It can also override a proxy set on the session, since it is added to the request-level dict first. Also, if `proxies` is `None`, the environment proxies are discarded at this step.
- `select_proxy` and `resolve_proxies` pick the final proxy later, at send time.

**CA bundle**
- If `verify` is `True` or `None`, `REQUESTS_CA_BUNDLE` wins, then `CURL_CA_BUNDLE`, then the original value.
- An explicit path or `False` passed to the call is left alone.
- `merge_setting(verify, self.verify)` runs last. Because the environment value replaces `verify` before that merge, it overrides `session.verify`.

### requests-env-settings/aab2

The merge happens in `Session.merge_environment_settings` (sessions.py), called from `Session.request`. It is skipped entirely if `trust_env=False`.

**Proxies:** `get_environ_proxies` (utils.py) supplies the env values. They are added to the request's `proxies` dict with `setdefault`, so they never overwrite explicit request proxies. The result is then combined with `Session.proxies` via `merge_setting`. Precedence per scheme:
1. Explicit `proxies=` argument.
2. Environment (`HTTP_PROXY` etc.).
3. `Session.proxies`.

`should_bypass_proxies` handles `NO_PROXY`, and `select_proxy` picks the most specific key later.

**CA bundle:** `REQUESTS_CA_BUNDLE`, then `CURL_CA_BUNDLE`, replace `verify` only when it is `True` or `None`. Precedence:
1. Explicit `verify=False` or a path.
2. Environment variables. These also override an explicit `verify=True`.
3. `Session.verify`.
4. Default `True`, which uses the certifi bundle (applied in `HTTPAdapter.cert_verify`).

On redirects, `rebuild_proxies` re-resolves proxies.

### requests-env-settings/03c6

The merge happens in `Session.merge_environment_settings` (`src/requests/sessions.py:831`), which `Session.request` calls.

**Proxies:** when `trust_env` is true, `get_environ_proxies` (utils.py) reads the environment, honouring `no_proxy`. Its results are added to the per-request `proxies` with `setdefault`, so per-request proxies beat environment ones. Then `merge_setting(proxies, self.proxies)` combines the result with session proxies, and request-level values override session-level ones.

Precedence for proxies:
1. Per-request `proxies`
2. Environment proxies
3. `Session.proxies`

Environment proxies are only added when `proxies` is not `None`. When it is `None`, they go in through the session merge instead.

**CA bundle:** the environment is consulted only if `verify` is `True` or `None`. It then takes `REQUESTS_CA_BUNDLE`, then `CURL_CA_BUNDLE`, then the original `verify`. After that, `merge_setting(verify, self.verify)` runs. So an explicit per-request `verify=False` or a path beats the environment, while the environment beats `Session.verify`.

At send time, `resolve_proxies` and `rebuild_proxies` re-apply environment proxies for redirects.

## requests-hooks-cookies

In requests, what happens to cookies set by a response, and how do response hooks run?

Points:
1. Session.send calls dispatch_hook('response', hooks, r) right after the adapter returns
2. dispatch_hook calls each hook in turn, and a hook that returns a value replaces the response passed on
3. Session hooks and request hooks are merged in prepare_request (merge_hooks)
4. After the hooks, extract_cookies_to_jar stores the response cookies in the session's cookie jar (also for responses in history)
5. During redirects, cookies are extracted again and the next request's Cookie header is rebuilt from the jar

### requests-hooks-cookies/1d8d

**Cookies:** `HTTPAdapter.build_response` calls `extract_cookies_to_jar`, which fills `Response.cookies`. That function wraps the request and response in `MockRequest` and `MockResponse` and calls `jar.extract_cookies`. After the hooks run, `Session.send` calls `extract_cookies_to_jar` for each response in `r.history` and for `r.raw`, so the cookies end up in `self.cookies`. `resolve_redirects` also extracts cookies at each hop, merges them into the next request with `merge_cookies`, and rebuilds the `Cookie` header through `prepare_cookies`. On later requests, `Session.prepare_request` merges the session and per-request cookies, and `get_cookie_header` builds the header.

**Hooks:** You register hooks with `register_hook`, or by passing `hooks=`. `prepare_hooks` copies them onto the `PreparedRequest`, and `merge_hooks` combines them with the session's hooks. In `Session.send`, `dispatch_hook("response", hooks, r, **kwargs)` runs right after `adapter.send`. This is before cookie persistence and redirect handling. Each hook is called as `hook(response, **kwargs)`, and a non-`None` return value replaces the response. `HTTPDigestAuth` uses this mechanism to register `handle_401` as a response hook.

### requests-hooks-cookies/e3e5

**Cookies:** `HTTPAdapter.build_response` (adapters.py:395) calls `extract_cookies_to_jar` to copy the `Set-Cookie` headers into `response.cookies`. Then `Session.send` calls `extract_cookies_to_jar(self.cookies, request, r.raw)` (sessions.py:799), which persists them in the session's cookie jar. During redirects, `Session.resolve_redirects` (sessions.py:267, 303) also extracts cookies from each intermediate response into the jar before building the next request.

**Hooks:** `Session.send` runs `dispatch_hook("response", hooks, r, **kwargs)` (sessions.py:791) right after the adapter returns the response. It happens before the session cookie extraction and before redirects are resolved. `dispatch_hook` (hooks.py:32) accepts a single callable or a list of them. It calls each one in order with the response. If a hook returns a non-None value, that value replaces the response passed to the next hook. It returns the final response.

### requests-hooks-cookies/c7b0

**Cookies:** `HTTPAdapter.build_response` calls `extract_cookies_to_jar` to fill `response.cookies`. Then `Session.send` extracts the cookies into the session jar, `self.cookies`. It does this for any `r.history` responses and for the final response. `Session.resolve_redirects` repeats the extraction for each hop. It also merges the session jar into the next request's jar with `merge_cookies`, then calls `prepare_cookies` to build the `Cookie` header. `prepare_request` merges the session and per-request cookies, using `cookiejar_from_dict` and `merge_cookies`.

**Hooks:** Hooks are registered with `Request.register_hook`. `merge_hooks` combines the request and session hooks in `prepare_request`. The only call to `dispatch_hook("response", ...)` is in `Session.send`, right after the adapter returns. That is before cookie persistence and redirect handling. Each hook runs in order, and a non-`None` return value replaces the response. Every redirect hop goes through `send`, so hooks run on each hop.

### requests-hooks-cookies/34fc

**Cookies**
- `HTTPAdapter.build_response` (adapters.py) calls `extract_cookies_to_jar` to fill `response.cookies` from the raw urllib3 response.
- `Session.send` (sessions.py ~L791–799) then calls `extract_cookies_to_jar(self.cookies, request, r.raw)`. This persists the cookies in the session jar, so later requests send them.
- During redirects, `Session.resolve_redirects` calls `extract_cookies_to_jar` on each intermediate response. It updates the jar as it goes, and the redirected request's cookies are rebuilt from it.
- `Session.send` also extracts cookies from any redirect-history responses.

**Hooks**
- `Request.register_hook` (models.py) adds callbacks to `hooks["response"]`.
- `Session.send` calls `dispatch_hook("response", hooks, r, **kwargs)` right after the adapter returns the response, and before the cookie extraction into the session jar.
- `dispatch_hook` (hooks.py) iterates over the registered callables. Each is called as `hook(hook_data, **kwargs)`. If a hook returns a non-`None` value, that value replaces the response passed to the next hook.

### requests-hooks-cookies/1c89

**Cookies:** In `HTTPAdapter.build_response` (adapters.py:395), `extract_cookies_to_jar` copies the raw response's `Set-Cookie` headers into `response.cookies`. In `Session.send` (sessions.py:~799), `extract_cookies_to_jar(self.cookies, request, r.raw)` then merges them into the session's cookie jar. During redirects, `Session.resolve_redirects` (lines 267 and 303) also extracts cookies from each intermediate response. It does this so later requests in the chain send them.

**Hooks:** `Request.register_hook` (models.py:257) adds callbacks to the request's `hooks` dict. After the adapter returns the response, `Session.send` calls `dispatch_hook("response", hooks, r, **kwargs)` (sessions.py:791). `dispatch_hook` (hooks.py:32) accepts either one callable or a list. It calls each callback with the response, and if a callback returns something other than `None`, that value replaces the response passed to the next one. It returns the final response.

The hook dispatch comes before the redirect and cookie handling in `Session.send`. I did not read the code between lines 791 and 799, so I haven't confirmed the exact ordering there.

### requests-hooks-cookies/c113

**Cookies:** `HTTPAdapter.build_response` (adapters.py:395) calls `extract_cookies_to_jar` to put `Set-Cookie` values into the response's own `r.cookies` jar. Later, `Session.send` (sessions.py:799) calls `extract_cookies_to_jar` again to save them in the session jar (`self.cookies`). On redirects, `resolve_redirects` copies the request, removes the `Cookie` header, calls `merge_cookies` with the session jar, and calls `prepare_cookies` to rebuild the header. It then extracts the hop's cookies into the session jar (sessions.py:303). On later requests, `prepare_request` merges session and request cookies with `merge_cookies`. Request-level cookies win on name collisions.

**Hooks:** `register_hook` and `prepare_hooks` (models.py) attach hooks to the request. "response" is the only valid event. `Session.send` calls `dispatch_hook("response", hooks, r, **kwargs)` (sessions.py:791) after `adapter.send` and before the response's cookies are saved to the session jar. `dispatch_hook` (hooks.py:32) calls each hook as `hook(data, **kwargs)`. A non-None return replaces the response.

### requests-hooks-cookies/c738

**Cookies:** `HTTPAdapter.build_response` (adapters.py) calls `extract_cookies_to_jar` to put the response's cookies into `response.cookies`. Then `Session.send` (sessions.py) calls `extract_cookies_to_jar(self.cookies, request, r.raw)`, which persists them in the session's cookie jar. For any redirect history entries, it also extracts cookies from each `resp.request`. During redirects, `Session.resolve_redirects` extracts cookies again into the session jar and the prepared request's jar. Each follow-up request is then built with those cookies.

**Hooks:** In `Session.send`, after the adapter returns the response, `dispatch_hook("response", hooks, r, **kwargs)` (hooks.py) runs. It accepts either a single callable or a list. Each hook is called as `hook(r, **kwargs)`. If a hook returns a non-None value, that value replaces the response for the next hook. The hooks come from the request's `hooks` dict, which `Session.prepare_request` and `Session.request` merge via `merge_hooks`.

### requests-hooks-cookies/a95e

**Cookies:** `HTTPAdapter.build_response` (adapters.py:395) calls `extract_cookies_to_jar` to fill `Response.cookies`. That function wraps the request and response in `MockRequest` and `MockResponse` and calls `jar.extract_cookies`. After that, `Session.send` (sessions.py:797-799) calls `extract_cookies_to_jar` again to store the cookies in the session jar, `self.cookies`. It does this for any `r.history` responses too. `resolve_redirects` extracts cookies on each hop, merges them with `merge_cookies`, and rebuilds the `Cookie` header with `prepare_cookies`.

**Hooks:** `default_hooks()` creates the registry, and only the `response` event exists. `register_hook` adds callables, from `Request.__init__` and `prepare_hooks`. `Session.prepare_request` combines request and session hooks with `merge_hooks`. In `Session.send`, `dispatch_hook("response", hooks, r, **kwargs)` (sessions.py:791) runs right after `adapter.send`. Each hook is called as `hook(data, **kwargs)`, and a non-None return value replaces the response. The session cookie extraction happens after the hooks run.

## diff-tour-flow

Trace what happens from running `tour feature/x` until the tour appears on screen: which functions run, in order?

Points:
1. main.run parses flags, loads the config and resolve() turns a branch name into base = the default branch and head = the branch
2. ui.NewGenerating / App.start computes a CacheKey and opens the saved tour with LoadCached if there is one
3. Otherwise generate.Generate runs in a goroutine: PrepareWorkspace exports the merge base and head with git archive into base/ and head/ plus change.diff and commits.txt, and writes the lessons
4. buildPrompt assembles the prompt and backend.Run runs the LLM CLI (for Claude Code, claude -p with stream-json) in that workspace
5. The reply is parsed and validated (settle, with up to 2 repair rounds), saved with SaveCached, and openViewer shows it

### diff-tour-flow/fca8

`tour feature/x` runs these functions, in this order:

1. `main` calls `run` in `cmd/tour/main.go`.
2. `run` calls `parse` for the flags, then `config.Load`, `i18n.Detect`, `i18n.Parse` and `i18n.Set`.
3. `git.Open` and `repo.RevParse("HEAD")` open the repo and check it. `isTerminal` checks for a TTY. `runSetup` runs only if there is no config.
4. `backendFor` picks the LLM backend.
5. `resolve` turns `feature/x` into a base and head commit.
6. `ui.NewGenerating` builds the app, and `tea.NewProgram(app).Run()` starts it.
7. `App.Init` calls `App.start`.
8. `start` first calls `generate.LoadCached`. On a cache hit it calls `openViewer`, which calls `NewViewer`, and the tour shows.
9. On a miss, `start` calls `generate.WorkDir`, then runs `generate.Generate` in a goroutine.
10. `Generate` calls `PrepareWorkspace`, `writeLessons`, `buildPrompt` and `backend.Run`. It then calls `settle`, which loops over `parse` and `repairPrompt`, calling `backend.Run` again to repair.
11. The goroutine calls `SaveCached` and sends a `doneMsg`.
12. `App.Update` handles the `doneMsg` by calling `openViewer`, which calls `NewViewer`. `App.Update` and `View` then render the viewer, and the tour shows.

### diff-tour-flow/ca02

Running `tour feature/x` goes through these steps, in order:

1. `main` calls `run`. It parses flags with `parse`, then calls `config.Load`, `i18n.Parse` and `i18n.Set`.
2. `run` opens the repo with `git.Open`. It checks `repo.RevParse("HEAD")` and `isTerminal`, and runs `runSetup` if there is no config.
3. `backendFor` picks the LLM backend, for example `generate.ClaudeCode`.
4. `resolve` turns `feature/x` into a base and head. The base comes from `repo.DefaultBranch`.
5. `ui.NewGenerating` computes `generate.CacheKey`. `tea.NewProgram(app).Run()` then calls `App.Init`, which calls `App.start`.
6. `start` calls `generate.LoadCached`. On a cache hit it goes straight to `openViewer`, which calls `NewViewer`, and the tour appears.
7. On a miss, `start` calls `generate.WorkDir` and launches a goroutine running `generate.Generate`. `Generate` runs `PrepareWorkspace`, `writeLessons` and `buildPrompt`, then `backend.Run`. It then calls `extractJSON` and `settle`, which calls `parse` and repairs the tour via `backend.Run` if validation finds problems.
8. The goroutine calls `generate.SaveCached` and sends `doneMsg`. `App.Update` handles it by calling `openViewer`, which calls `NewViewer`, and the tour appears.

While it waits, `waitFor` and `tick` drive the progress screen.

### diff-tour-flow/964d

Starting from `main`, `tour feature/x` runs these steps in order:

1. `main` calls `run`.
2. `run` calls `parse` for the flags, then `config.Load`, `i18n.Detect` / `i18n.Parse` / `i18n.Set`, and `git.Open`.
3. `run` checks `repo.RevParse("HEAD")` and `isTerminal`. If there is no config, it calls `runSetup`.
4. `run` calls `backendFor(cfg.LLM)`, for example `generate.ClaudeCode`.
5. `run` calls `resolve`, which uses `repo.RevParse` and `repo.DefaultBranch`. It returns the base (default branch) and head (`feature/x`).
6. `run` calls `ui.NewGenerating`, which calls `generate.CacheKey`.
7. `run` calls `tea.NewProgram(app).Run()`, which calls `App.Init`, which calls `App.start`.
8. `start` calls `generate.LoadCached`.
   - **Cache hit:** `openViewer` calls `generate.WorkDir` and `NewViewer`. The tour is shown immediately.
   - **Cache miss:** `start` calls `generate.WorkDir` and launches a goroutine.
9. The goroutine calls `generate.Generate`. That calls `PrepareWorkspace`, `writeLessons`, `buildPrompt`, `backend.Run`, then `extractJSON` and `settle`. `settle` calls `parse` and, if there are problems, `repairPrompt` and `backend.Run` again.
10. The goroutine calls `generate.SaveCached` and sends `doneMsg`.
11. `App.Update` handles `doneMsg` and calls `openViewer`, which calls `NewViewer`. The tour is rendered.

I didn't open `App.Update`, so step 11 is inferred from `doneMsg` being sent on the channel.

### diff-tour-flow/47de

Here is the order of calls for `tour feature/x`, from `cmd/tour/main.go` and `internal/ui/app.go`.

1. `main` calls `run(os.Args[1:])`.
2. `run` parses the flags with `parse`, then calls `config.Load`, `i18n.Detect`/`i18n.Parse` and `i18n.Set`.
3. `run` calls `git.Open`, then `repo.RevParse("HEAD")`, and checks `isTerminal`. If there is no config, it calls `runSetup`.
4. `run` calls `backendFor(cfg.LLM)`, then `resolve(repo, "feature/x")` to get base and head.
5. `ui.NewGenerating` calls `generate.CacheKey` and sets the generating screen.
6. `tea.NewProgram(app).Run()` calls `App.Init`, which calls `App.start`.
7. `start` calls `generate.LoadCached`. On a hit it calls `openViewer` (which uses `generate.WorkDir` and `NewViewer`) and the tour shows at once.
8. On a miss, `start` calls `generate.WorkDir` and launches a goroutine that runs `generate.Generate`. This calls `PrepareWorkspace`, `writeLessons`, `buildPrompt`, `backend.Run` (retried once if the reply has no JSON), then `settle`. `settle` runs `parse` and repair passes.
9. After `Generate` returns, the goroutine calls `SaveCached` and sends a `doneMsg`. Meanwhile `waitFor` and `tick` drive the progress screen.
10. `App.Update` handles `doneMsg` by calling `openViewer`, which builds the `Viewer` that shows the tour.

### diff-tour-flow/b452

Running `tour feature/x` goes through these functions, in order:

1. `main()` calls `run()` in `cmd/tour/main.go`.
2. `run()` calls `parse()` for the flags, `config.Load()` (with `runSetup` if no config exists), `git.Open`, and `backendFor()`, which returns `generate.ClaudeCode{}` for `claude-code`.
3. `resolve()` turns the branch name into a base (`DefaultBranch()`) and a head (`feature/x`).
4. `ui.NewGenerating()` builds the app, and `tea.NewProgram(...).Run()` starts it.
5. `App.Init()` calls `App.start()`. `generate.LoadCached` runs first. On a hit it goes straight to `openViewer`. On a miss, a goroutine calls `generate.Generate()`.
6. `Generate()` calls `PrepareWorkspace` (`git archive` exports plus `RawDiff`), then `buildPrompt`, then `ClaudeCode.Run()`.
7. The reply goes through `settle()`, `parse()`, `extractJSON`, `Tour.Validate()` and `render.Check`, with repair prompts if any of these fail.
8. `SaveCached` writes the result and a `doneMsg` is sent.
9. `App.Update()` handles `doneMsg` and calls `openViewer()`, which calls `NewViewer`. `App.View()` then delegates to `Viewer.View()`, which draws the tour.

### diff-tour-flow/a307

Flow for `tour feature/x`, as traced by the Explore agent. It did not read the bodies of `Viewer.Update`, `NewViewer` or `Tour.Steps`.

1. `main` → `run` → `parse` (flags and args), then `config.Load`, `i18n.Detect`, `git.Open`, `backendFor`.
2. `resolve` finds `feature/x` and the default branch through `Repo.DefaultBranch`.
3. `ui.NewGenerating` calls `generate.CacheKey`. `tea.NewProgram(...).Run()` then starts the UI.
4. `App.Init` → `App.start` → `generate.LoadCached`. On a hit it goes straight to `openViewer`.
5. On a miss, a goroutine runs `generate.Generate`:
   - `PrepareWorkspace` runs `MergeBase`, `Archive`/`extractArchive` and `RawDiff`.
   - `writeLessons` and `buildPrompt` run next.
   - `backend.Run`, for example `ClaudeCode.Run`, calls the LLM.
   - `settle` runs `parse`, `extractJSON`, `Tour.Validate` and `render.Check`. If these find problems, `repairPrompt` plus another `Run` repairs the tour.
   - `SaveCached` saves the result.
6. `doneMsg` arrives and `App.Update` calls `openViewer`, which calls `NewViewer`.
7. `Viewer.View` calls `render.Frame` and the block renderers. The tour appears on screen.

### diff-tour-flow/7985

Running `tour feature/x` goes through these steps in order:

1. `main` calls `run` in `cmd/tour/main.go`.
2. `run` calls `parse` for the flags, then `config.Load`, `i18n.Detect`/`i18n.Parse`/`i18n.Set`, and `git.Open`.
3. It checks `repo.RevParse("HEAD")` and `isTerminal`. If there is no config, it calls `runSetup`.
4. `backendFor(cfg.LLM)` picks the LLM backend.
5. `resolve(repo, "feature/x")` returns the base (the default branch) and the head.
6. `ui.NewGenerating` calls `generate.CacheKey` and creates the `App` on the generating screen.
7. `tea.NewProgram(app).Run()` starts Bubble Tea, which calls `App.Init`, and that calls `App.start`.
8. `start` calls `generate.LoadCached`. On a cache hit it calls `openViewer`, which calls `NewViewer`, and the tour appears.
9. On a miss, `start` calls `generate.WorkDir` and launches a goroutine.
10. The goroutine runs `generate.Generate`. That calls `PrepareWorkspace`, `writeLessons`, `buildPrompt`, `backend.Run` (the LLM streams events through `progress`), and `settle`. `settle` calls `parse`, and repairs the tour if validation finds problems.
11. Then `SaveCached` runs and a `doneMsg` is sent.
12. `App.Update` handles `doneMsg` by calling `openViewer`/`NewViewer`, and the viewer renders the tour.

### diff-tour-flow/6155

`main` calls `run`, which parses flags with `parse`, loads config, and opens the repo with `git.Open`. It checks `RevParse("HEAD")` and `isTerminal`, running `runSetup` if no config exists. `backendFor` picks `generate.ClaudeCode`, and `resolve` turns `feature/x` into base (`DefaultBranch`) and head.

`ui.NewGenerating` builds the cache key with `generate.CacheKey`, then `tea.NewProgram(app).Run()` starts. `App.Init` calls `start`. If `generate.LoadCached` finds a saved tour, it opens the viewer. Otherwise a goroutine runs `generate.Generate`.

`Generate` calls `PrepareWorkspace` (which uses `extract` and `git archive`), then `writeLessons` and `buildPrompt`. `ClaudeCode.Run` calls `claude` with `claudeArgs`, and the prompt goes on stdin. `settle` parses the reply with `parse`, which runs `extractJSON`, `Validate` and `render.Check`, and repairs it if needed. `SaveCached` stores the result, and the goroutine sends `doneMsg`. `App.Update` handles it by calling `openViewer`, and `App.View` then delegates to `Viewer.View`.

## diff-tour-invalid

What does diff-tour do when the LLM's reply is not a valid tour?

Points:
1. extractJSON cuts the reply at a separator line and takes the text from the first { to the last }
2. If the reply has no JSON at all, the generation is run once more instead of asking for a fix
3. parse decodes with unknown fields disallowed, runs Validate, and render.Check verifies that referenced files, line ranges and hunks exist
4. The problems are sent back in a repair prompt, with the previous JSON written to draft.txt in the workspace
5. At most MaxRepairs = 2 repair rounds, then it fails with InvalidTourError listing the problems

### diff-tour-invalid/bba0

When the reply isn't a valid tour, diff-tour tries to repair it, and if that fails it shows an error and stops.

- **Checking:** `parse()` in `internal/generate/generate.go` pulls the JSON out with `extractJSON`. It then decodes it strictly and runs `tour.Tour.Validate()` and `render.Check`. Each check returns plain-text problem strings.
- **Empty reply:** if `Generate()` finds no JSON at all, it re-runs the original prompt once.
- **Repair:** `settle()` re-prompts the LLM with `repairPrompt(problems)` and re-parses the result. It does this up to `MaxRepairs = 2` times. The UI shows "Fixing problems in the tour".
- **Final failure:** after that, `settle()` returns `InvalidTourError`. The message reads "the generated tour is still invalid: …". There is no fallback tour and nothing is cached.
- **What the user sees:** in `internal/ui/app.go`, `fail()` sets the failure text and `screenFailed` shows it in red under "Could not write the tour". The only key offered is `q` to quit.

### diff-tour-invalid/0980

When the LLM's reply isn't a valid tour, diff-tour validates it, asks the LLM to fix it, and gives up with an error after a few tries. The code is in `internal/generate/generate.go`.

- **`Generate`** runs the backend once. If `extractJSON` finds no JSON object at all, for example because the reply stopped midway, it re-runs the original prompt once instead of asking for a fix.
- **`settle`** loops over each reply and calls **`parse`**.
  - `parse` pulls the JSON out with `extractJSON`, decodes it with unknown fields disallowed, and pins base and head to the requested values. It then calls `tour.Validate`, `DropRevealingAsks` and `render.Check`, which verify the format and that the referenced files and lines exist. It returns a list of problems.
  - If there are problems, `settle` writes the JSON to `draft.txt` in the work directory. It then runs the backend with **`repairPrompt`**, which lists the problems and asks only for those to be fixed. It reports a "repairing" progress phase and counts the repair in the stats.
- After `MaxRepairs` attempts, `settle` returns an **`InvalidTourError`** carrying the remaining problems.

### diff-tour-invalid/d93c

diff-tour runs a parse, validate and repair loop in `internal/generate/generate.go`.

- `Generate` runs the backend. If `extractJSON` finds no JSON object, it re-runs the whole prompt once.
- `parse` decodes the JSON with unknown fields disallowed and pins base and head. It then calls `Tour.Validate` (`internal/tour/tour.go`) for structure. After that, `render.Check` confirms the referenced files and line ranges exist.
- `settle` handles problems. It writes the draft to `draft.txt` and sends the same agent `repairPrompt(problems)`. It does this at most `MaxRepairs` (2) times. Each reply is parsed from scratch.
- If problems remain, `settle` returns `InvalidTourError` ("the generated tour is still invalid: …").
- `App.start` passes the error to `a.fail`, which shows the failure screen. Nothing is cached.
- In ask mode, `Answer` uses the same loop, but the UI only shows a one-line error.
- An invalid cached tour from `LoadCached` is treated as a miss and regenerated.

### diff-tour-invalid/7786

`Generate` runs the model and passes each reply to `settle`. `settle` calls `parse`, which does the following:

- `parse` uses `extractJSON` to pull the JSON out of the reply. It then decodes it, which gives an "invalid JSON: …" problem if that fails. It then checks the result with `tour.Validate` and `render.Check`, which find format and reference errors.
- If there are problems, `settle` reports "repairing" progress and increments `stats.Repairs`. It then calls `backend.Run` with `repairPrompt(problems)`, which asks the model to fix only those errors. The earlier JSON is placed in `draft.txt` in the working directory.
- This repeats up to `MaxRepairs` (2) times per reply.
- If problems remain after that, `settle` returns `*InvalidTourError`. Its `Error()` message is "the generated tour is still invalid: …", followed by the problems. That error goes back through `Generate` to the caller, and the invalid tour is not used.

### diff-tour-invalid/0df8

In `internal/generate/generate.go`, `Generate` first checks the reply with `extractJSON`. If the reply contains no JSON object, for example because it stopped midway, `Generate` runs the backend a second time with the same prompt.

`settle` then handles the reply. It calls `parse`, which does four things:
- It extracts the JSON with `extractJSON`.
- It decodes the JSON strictly, with `DisallowUnknownFields`.
- It runs `tour.Validate()`.
- It runs `render.Check`, which confirms the referenced files and lines exist.

`parse` returns a list of problems.

If there are problems, `settle` writes the earlier JSON to `draft.txt` and calls `backend.Run` with `repairPrompt(problems)`. It reports a "repairing" progress phase and counts the repair in `Stats`. It does this up to `MaxRepairs` (2) times.

If problems remain after that, `settle` returns `*InvalidTourError`, whose message is "the generated tour is still invalid: …". `Generate` passes that error back to the caller.

### diff-tour-invalid/71b9

`Generate` (`internal/generate/generate.go`) runs the backend, then checks the reply.

- **No JSON:** if `extractJSON` finds no object, `Generate` re-runs the same prompt once.
- **Validation:** `settle` calls `parse`, which decodes the JSON strictly (unknown fields rejected). It then runs `Tour.Validate` and `render.Check`, which confirms the referenced files, lines and hunks exist.
- **Repair:** if `parse` reports problems, `settle` sends the LLM a `repairPrompt(problems)`. It does this up to `MaxRepairs` (2) times and re-parses each reply.
- **Failure:** if problems remain, `settle` returns `InvalidTourError`. The message is "the generated tour is still invalid: …", followed by the problems.
- **UI:** `internal/ui/app.go` receives the error and calls `fail`, which shows the failure screen. The result isn't cached.

The `ask` path (`ask.go`) also uses `settle`, but it has no no-JSON retry.

I didn't run anything. This comes from the subagent's reading of the code.

### diff-tour-invalid/0314

When the reply isn't a valid tour, diff-tour retries and then shows an error.

- **No JSON found:** `Generate` calls `extractJSON`. If it returns nil, `Generate` reruns the whole prompt once.
- **Checking the reply:** `parse` decodes the JSON strictly and rejects unknown fields. It then runs `Tour.Validate` for the structure and `render.Check` to confirm that the referenced files, line ranges and hunks exist. It returns a list of problems.
- **Repair:** `settle` loops. If problems remain, it sends `repairPrompt(problems)` back to the model, up to `MaxRepairs` = 2 times.
- **Final failure:** `settle` returns `InvalidTourError`, which reads "the generated tour is still invalid: …". In `internal/ui/app.go`, `fail` then shows a red "Could not write the tour" screen with the problem list. Nothing is cached.
- **Ask flow:** `Answer` also uses `settle`, and an answer with no steps becomes an `InvalidTourError`.
- **Backend errors:** a process or CLI failure skips the repair loop and fails straight away.

### diff-tour-invalid/52d3

When the reply isn't a valid tour, diff-tour asks the LLM to fix it, up to twice, and then fails.

- `Generate` hands each reply to `settle`.
- `settle` calls `parse`. `parse` uses `extractJSON` to strip fences, preamble and research notes, then decodes strictly. Unknown fields are rejected. After that it runs `tour.Validate` and `render.Check`, which check the format and that the referenced files and lines exist. It returns a list of problems.
- If there are problems and fewer than `MaxRepairs` (2) repairs have been tried, `settle` does the following:
  - It increments `stats.Repairs`.
  - It reports a "repairing" `Progress`.
  - It writes the draft JSON to `draft.txt` in the work dir.
  - It calls `backend.Run` with `repairPrompt(problems)`, which asks for only those errors to be fixed.
  - It adds the repair cost to `stats.CostUSD`.
  - It re-parses the new reply.
- After two failed repairs it returns `*InvalidTourError`, whose message is "the generated tour is still invalid: …" followed by the problems.
- A backend error during a repair is returned immediately.

## diff-tour-sandbox

How does diff-tour keep the LLM from changing the user's repository or reading files outside it?

Points:
1. The LLM works in an exported copy (a workspace in the cache directory made with git archive), not in the repository
2. Extraction skips absolute paths and paths that climb out with .., and does not export symlinks
3. For Claude Code it passes --restricted and --tools Read,Grep,Glob (read-only tools only)
4. It also passes --permission-mode dontAsk and --strict-mcp-config (no MCP servers, no prompts)
5. The workspace is deleted when generation finishes

### diff-tour-sandbox/af8b

diff-tour never lets the LLM touch the real repo. `PrepareWorkspace` (`internal/generate/workspace.go`) exports `base/` and `head/` copies into a temp directory. It does this with `extract`, which calls `Repo.Archive` (`git archive`, with `GIT_OPTIONAL_LOCKS=0`). `extractArchive` keeps only regular files and directories, so symlinks are dropped, and it rejects absolute paths and `..`. For a live working tree, `copyWorktree` copies only tracked and non-ignored files. Each backend's `Run` sets `cmd.Dir` to that copy, and `Workspace.Remove` deletes it afterward.

Each backend is also restricted to read-only tools:
- **Claude:** `claudeArgs` passes `--restricted`, `--tools Read,Grep,Glob` and `--permission-mode dontAsk`.
- **Codex:** `codexArgs` builds a sandbox profile that can read only the workspace and system paths. It sets `approval_policy="never"` and fails closed through `sandboxBroken`.
- **OpenCode:** `openCodeConfig` denies every tool except read, glob and grep. `openCodeEnv` blocks user and project config.
- **Copilot:** `copilotArgs` denies shell and write. `prepareCopilotHome` and `copilotEnv` strip risky user settings.

The Codex and Copilot sandboxes rely on each CLI's own enforcement, and the code comments say they were checked only on specific CLI versions. The Codex sandbox was verified on Linux (bubblewrap) only.

### diff-tour-sandbox/6414

diff-tour never lets the LLM near the real repo. `Generate` (`internal/generate/generate.go`) calls `PrepareWorkspace` (`workspace.go`). That function exports the base and head versions into a temp directory with `Repo.Archive`, which runs read-only `git archive` with `GIT_OPTIONAL_LOCKS=0`. A working-tree head goes through `copyWorktree` instead. `extractArchive` skips absolute and `..` paths and writes only regular files and directories, so symlinks are never exported. Each backend runs with its cwd set to that copy, and `ws.Remove()` deletes the copy afterwards.

Each backend is also restricted to reading:
- **Claude Code:** `claudeArgs` passes `--restricted`, `--tools Read,Grep,Glob` and `--permission-mode dontAsk`.
- **Codex:** `codexArgs` uses a custom read-only permission profile (`diff_tour_read`) and `approval_policy="never"`. It also disables the tools that reach outside the workspace.
- **OpenCode:** `openCodeConfig` denies everything except read, glob and grep.
- **Copilot:** `copilotArgs` passes `--deny-tool=shell` and `--deny-tool=write`. `prepareCopilotHome` strips unsafe keys from the user's config.

I didn't find a check after the run that the user's repo is unchanged. The Explore subagent didn't open `cmd/tour/main.go` or the tests, so one could exist there. The subagent also didn't confirm whether `ListFiles` and `Show` filter symlinks in the worktree-copy case.

### diff-tour-sandbox/9c8b

diff-tour combines read-only agent settings with a snapshot copy of the code, so the LLM never touches the real repository.

- **Snapshot:** The `Repo` methods in `internal/git/git.go` only run git commands that change neither the working tree nor `.git`. `Repo.Archive` exports the ref as a `git archive`. The agent runs with its working directory (`cmd.Dir = dir`) set to that extracted copy, not your checkout.
- **Claude Code:** `claudeArgs` passes `--restricted`, `--tools Read,Grep,Glob`, `--strict-mcp-config` and `--permission-mode dontAsk`. These remove shell and web tools and MCP servers, confine file tools to the working directory, and deny anything that would need approval.
- **Codex:** `codexArgs` defines a sandbox profile (bubblewrap or Seatbelt) that can read only the working directory and has nothing writable. `codexReadablePaths` adds the paths the codex binary needs to run inside it. If the sandbox can't start, `Run` fails rather than running unsandboxed.
- **Other CLIs:** `geminiArgs`, `openCodeArgs`, `copilotArgs` and their helpers (`geminiEnv`, `openCodeEnv`, `copilotEnv`) use read-only tool lists and deny-by-default policies. They run with empty config directories, so your own settings, hooks and extensions aren't loaded.

### diff-tour-sandbox/3895

diff-tour never lets the LLM CLI touch the real repository. It gives the CLI a throwaway export and limits it to read-only tools.

**Isolation**
- `Repo.Archive` runs `git archive` with `GIT_OPTIONAL_LOCKS=0`, so it only reads.
- `PrepareWorkspace` builds `base/` and `head/` trees in a temp dir. For uncommitted changes, `copyWorktree` copies only tracked, non-ignored files.
- `extractArchive` rejects absolute or `..` paths and skips symlinks.

**Read-only tools**
- `claudeArgs` allows only `Read,Grep,Glob`, with `--permission-mode dontAsk`, and runs in the export directory.
- `geminiPolicy` writes a policy that allows only read tools and denies paths outside the workspace.
- `openCodeConfig` denies everything except `read`, `glob` and `grep`.
- `copilotArgs` sets `--available-tools` to view, grep, rg and glob, and denies shell and write.
- `codexArgs` uses a read-only filesystem profile, and `Run` fails closed if the sandbox can't start.

**Config isolation**
- `openCodeEnv`, `copilotEnv` and `prepareCopilotHome` point the CLIs at temporary config directories, so user config can't loosen the limits.

Claude has no env scrubbing or explicit path check, so it relies on its tool list and working directory.

### diff-tour-sandbox/4494

diff-tour never lets the LLM touch the real repository, and it limits each CLI to read-only tools.

**Copy, not the repo.** `PrepareWorkspace` builds a scratch directory. `extract` fills it with `base` and `head` snapshots, plus `change.diff` and `commits.txt`. A snapshot comes from `Repo.Archive` (`git archive`) via `extractArchive`, or from `copyWorktree` for uncommitted state. `extractArchive` writes only regular files and directories. The git package uses only commands that change neither the working tree nor `.git`. `Workspace.Remove` deletes the scratch directory afterwards.

**Read-only agents.** Each CLI runs in that directory with its own restrictions:
- `claudeArgs` passes `--restricted`, `--tools Read,Grep,Glob`, `--strict-mcp-config` and `--permission-mode dontAsk`.
- `codexArgs` uses a sandbox profile that can read only the working directory and write nothing. `Codex.Run` fails closed if the sandbox can't start (`sandboxBroken`).
- `geminiArgs` builds a read-only policy file and sets `GEMINI_SANDBOX=false`.
- `openCodeArgs` and `copilotArgs` also restrict the tools to read-only.

I read `claudeArgs` and the workspace code directly. I only skimmed the Codex and Gemini comments, and didn't open the OpenCode or Copilot code.

### diff-tour-sandbox/94d3

Two layers keep the LLM contained:

**1. Isolated copy of the repo.** `Repo.Archive` (internal/git/git.go) runs `git archive` for the ref being toured. `extractArchive` (internal/generate/workspace.go) unpacks it into a temporary directory, and each backend's `Run` is given that directory as `dir`. The model therefore never touches the real working tree or `.git`. The `git` package itself only runs commands that modify neither the working tree nor `.git`.

**2. Read-only backends.** Each backend limits what the CLI can do:
- `claudeArgs` (claude.go) passes `--restricted`, `--tools Read,Grep,Glob`, `--permission-mode dontAsk`, `--strict-mcp-config`. This removes command execution and WebFetch, and confines file tools to the working directory.
- `codexArgs` (codex.go) defines a filesystem permissions profile that lists only the readable paths.
- `openCodeArgs` (opencode.go) denies everything, then allows only a few read tools.
- `geminiArgs` (gemini.go) builds a read-only policy.
- The Copilot backend (`copilotArgs`, copilot.go) strips unsafe settings keys (`copilotUnsafeKey`) from the config it passes on.

### diff-tour-sandbox/54b3

diff-tour never lets the agent CLI run in your repository. `PrepareWorkspace` (`internal/generate/workspace.go`) wipes a cache directory, then exports the base and head trees into it. `extract` uses `git archive` for commits and `copyWorktree` for the working tree. `extractArchive` rejects absolute and `..` paths and drops symlinks. The agent runs with that copy as its working directory, and `Workspace.Remove` deletes it afterwards.

The git helpers in `internal/git` use only commands that don't modify the repo. `run` sets `GIT_OPTIONAL_LOCKS=0`, and `Archive` streams a tar.

Each agent is also restricted to read-only tools:
- **Claude:** `claudeArgs` passes `--tools Read,Grep,Glob` and `--restricted`.
- **Codex:** `codexArgs` uses a read-only, no-network profile.
- **OpenCode:** `openCodeConfig` denies everything except read, glob and grep.
- **Copilot:** `copilotArgs` and `copilotEnv` deny shell and write.
- **Gemini:** `geminiPolicy` denies paths outside the workspace.

I read this from the code and comments only. I didn't run any of the agent CLIs, so I haven't confirmed the flags work against real versions. Prompt injection can still steer the model within these limits.

### diff-tour-sandbox/73c2

diff-tour never lets the LLM touch the real working tree. It gives the model an exported copy of the diff and restricts the backend CLI to reading it.

- **Export:** `Repo.Archive` in `git/git.go` runs `git archive` on the ref. `Generate` in `generate/generate.go` puts that snapshot in `workDir`, and `writeLessons` adds the lesson files there. The git helpers (`Repo.run`, `Show`, `Diff` and the others) use only read-only commands.
- **Claude Code:** `claudeArgs` passes `--tools Read,Grep,Glob` and `--permission-mode dontAsk`. `ClaudeCode.Run` sets `cmd.Dir` to the snapshot directory.
- **Codex:** `codexArgs` defines a permissions profile whose filesystem access is read-only and limited to the snapshot. It also sets `approval_policy="never"` and `--ignore-rules`. `codexReadablePaths` adds the paths the codex binary needs to run inside the sandbox. `Codex.Run` fails closed and reports an error if the sandbox can't start.

I read the Claude Code and Codex code. I didn't check the other backends (Gemini, OpenCode, Copilot).

## diff-tour-cache

When does diff-tour reuse a saved tour instead of asking the LLM again, and what makes it regenerate?

Points:
1. CacheKey is a sha256 over the merge base, the head commit, the language, PromptVersion and the backend name (first 24 hex chars)
2. Saved tours live in the user cache directory under tour/tours/<key>.json
3. App.start loads the saved tour with LoadCached and opens it without calling the LLM
4. A new commit on the branch, a different language or LLM, or a bumped PromptVersion gives a new key and a new tour
5. The branch picker shows which targets are already cached, and answers to reader questions are saved back into the cached tour

### diff-tour-cache/8bcb

diff-tour reuses a saved tour when `generate.LoadCached(key)` finds a file for the key. The key comes from `generate.CacheKey` in `internal/generate/generate.go`, a truncated SHA-256 of:

- the merge-base (`repo.MergeBase(base, head)`)
- the head commit (`repo.RevParse(head)`)
- the language
- `PromptVersion`
- the backend name (the LLM)

The UI computes the key in `internal/ui/app.go` (around lines 38, 51 and 135) and in `cmd/tour/main.go:167`. Around `app.go:159`, it loads the cached tour and only generates a new one if none exists. After a fresh generation it stores the result with `SaveCached`. The file location comes from `CachePath` in the user cache directory.

Any change to those inputs gives a different key, so no saved file matches and the LLM is asked again. That covers a new commit on head, a moved merge-base, a different language, a different LLM backend, or a bumped `PromptVersion`. `PromptVersion` is bumped whenever the prompts change.

I found no explicit "force regenerate" option in the code I searched.

### diff-tour-cache/db81

A saved tour is reused when a JSON file already exists at the cache path for the tour's key. Otherwise a new tour is generated.

**Reuse:** `App.start` (`internal/ui/app.go`) calls `generate.LoadCached(t.Key)`. If that returns a tour, it opens it with `openViewer` and never calls the LLM. `FindTargets` uses the same check to mark targets as `Cached`. The files live under `os.UserCacheDir()/tour/tours/<key>.json`, resolved by `CachePath`.

**Regenerate:** `generate.CacheKey` hashes five values:
- the merge-base of base and head
- `RevParse(head)`
- the language
- `PromptVersion`
- the backend name

If any of these changes, the key changes and nothing matches. That means new commits on the branch, a different base, language or LLM backend, or a bump of `PromptVersion` (which the comment says to do whenever the prompts change). In that case `start` runs `generate.Generate` and then `SaveCached`.

I found no explicit "force regenerate" flag. Old cache files are not deleted; they are just no longer matched.

### diff-tour-cache/4242

I've traced the caching code and am about to give the answer. It's already in hand.

**Reuse:** `App.start` in `internal/ui/app.go` calls `generate.LoadCached(t.Key)`. If a saved tour exists for that key, it opens it with `openViewer` and never calls the LLM. The picker (`NewPicker`) uses the same lookup to mark targets as cached. `NewGenerating` computes the key for a ref given on the command line.

**Key:** `generate.CacheKey` hashes five things with SHA-256:
- `repo.MergeBase(base, head)`
- `repo.RevParse(head)`
- the language
- `PromptVersion`
- the backend name

For uncommitted changes, `RevParse` returns `worktreeID()` instead of a commit hash.

**Regeneration:** if any of those five change, the key changes and `LoadCached` returns nil. Examples are new commits on the head, a moved merge-base, a different `--lang` or LLM, a bumped `PromptVersion`, or different uncommitted content. `start` then generates a tour and stores it with `generate.SaveCached`. The files live under `os.UserCacheDir()`, via `CachePath`.

I didn't find a force-refresh flag in `cmd/tour/main.go`, though I only grepped for a few terms.

### diff-tour-cache/cec5

**Reuse:** `App.start` (internal/ui/app.go) calls `generate.LoadCached(key)`. If a saved tour exists, it opens it with `openViewer` and never calls the LLM. `LoadCached` returns `nil, nil` on a miss (`os.ErrNotExist`). The file lives at `<UserCacheDir>/tour/tours/<key>.json` (`CachePath`). `FindTargets` uses the same check to mark a target "ready".

**Regeneration:** `generate.CacheKey` is the first 24 hex characters of a sha256 over:
- `repo.MergeBase(base, head)`
- `repo.RevParse(head)`
- the language
- `PromptVersion` (currently `"v6"`)
- `backend.Name()`

If any of these changes, the key changes and `Generate` runs, then `SaveCached` writes the new tour. For uncommitted work, `RevParse` returns `worktreeID()`, which hashes HEAD plus the working-tree diff. Any edit therefore forces regeneration.

The diff text itself isn't hashed, and the model isn't in the key, only the backend name. I found no regenerate flag, TTL, or cache-delete action.

### diff-tour-cache/9e5f

Saved tours are reused whenever one exists for the same cache key. In `App.start` (`internal/ui/app.go`), `generate.LoadCached(t.Key)` returns a saved tour, and `openViewer` opens it without calling the LLM. If none is found, `generate.Generate` runs in a goroutine. Its result is stored with `generate.SaveCached`. `CachePath` puts the file at `<user cache dir>/tours/<key>.json`.

`generate.CacheKey` builds the key as a SHA-256 hash (first 24 hex characters). It hashes these inputs:
- the merge base of base and head, from `repo.MergeBase`
- the head commit, from `repo.RevParse`
- the language
- `PromptVersion`
- the backend name

Changing any of them gives a different key, so the tour is regenerated. That means:
- new commits on head or a moved merge base
- a different language
- a different LLM backend
- a bumped `PromptVersion`, which the code comment says must happen whenever the prompts change

The ask feature also writes back through `SaveCached`, which replaces the saved tour for that key.

I found no force-regenerate flag in `app.go` or `main.go`, though I only grepped for a few obvious terms.

### diff-tour-cache/f281

diff-tour reuses a saved tour when a cache file for the current key exists and loads cleanly. `App.start` (`internal/ui/app.go`) calls `generate.LoadCached(key)`. If that returns no error and a tour, it opens the viewer and skips the LLM. On a miss it runs `generate.Generate`, then `generate.SaveCached` stores the result.

`generate.CacheKey` builds the key as a sha256 over these values:
- the merge-base, from `MergeBase`
- the head commit, from `RevParse(head)`
- the language
- `PromptVersion` (currently `"v6"`, bumped by hand when the prompts change)
- the backend name

Changing any of them regenerates the tour.

For uncommitted work, `RevParse(Worktree)` returns `worktreeID()`, a hash of HEAD plus the working diff. Any edit therefore changes the key.

A cached file also regenerates if `LoadCached` or `tour.Load` fails, including when `Validate` rejects it, for example on a schema `Version` mismatch.

The model name isn't in the key. The Explore agent found no refresh flag and no TTL.

### diff-tour-cache/d790

diff-tour reuses a saved tour when a file already exists for its cache key. `(*App).start` in `internal/ui/app.go` calls `generate.LoadCached`. If that finds a tour, it opens the viewer without calling the LLM. Otherwise it runs `generate.Generate` and then `SaveCached`.

`generate.CacheKey` builds the key as a SHA-256 prefix over these inputs:
- the merge base, from `repo.MergeBase`
- the resolved head, from `repo.RevParse`
- the language
- `PromptVersion` (currently "v6")
- the backend name

`CachePath` puts the file under the user cache directory.

Regeneration happens only when one of those inputs changes:
- **Commits and base:** new commits, or a moved or rebased base.
- **Language and backend:** a different language or a different backend. A different model within the same backend does not change the key.
- **Prompt version:** bumping `PromptVersion`.
- **Uncommitted changes:** any edit changes the key, because `RevParse` returns `worktreeID()`, a hash of HEAD plus the working-tree diff.

There is no refresh flag, and nothing deletes old tours. A failed generation saves nothing, so the next open retries.

### diff-tour-cache/cb72

**Reuse:** `App.start` (`internal/ui/app.go:157`) calls `generate.LoadCached(key)`. If a valid tour file exists for that key, it opens it and makes no LLM call. `generate.Generate` never checks the cache itself.

**Key:** `CacheKey` (`generate.go:226`) hashes the merge-base (`repo.MergeBase`), the head (`repo.RevParse`), the language, `PromptVersion` (currently "v6") and the backend name. The model name is not part of the key.

**Regeneration happens when:**
- The head changes. A new commit gives a new hash. For uncommitted work, `worktreeID` hashes HEAD plus the working diff, so any edit changes the key.
- The merge-base, language or backend changes, or `PromptVersion` is bumped.
- The file is missing, or `tour.Load` → `Validate` rejects it, for example when `Version != 1`.
- Someone deletes the cache file by hand.

Once generation succeeds, `SaveCached` writes the tour. There is no refresh flag.

## diff-tour-ask

How does asking a question during a tour work in diff-tour, from typing the question to the new steps appearing?

Points:
1. The viewer opens an ask panel (with suggested questions) and startAnswer runs generate.Answer in the background
2. Answer prepares the same workspace as a tour and also writes the whole tour as tour.json
3. askPrompt lists the tour steps with the current one marked, includes the current step's JSON and the reader's question
4. The reply goes through the same parse and repair loop (settle) and its steps are taken out of the returned chapters
5. The new steps are inserted right after the current step (InsertAfter), marked with the question, and saved to the cached tour when possible

### diff-tour-ask/f48d

From typing the question to the new steps appearing, the flow is:

1. **`Viewer.openAsk`** opens the question panel. It shows the current step's suggested `Asks` and a free-text line. If no `Asker` was configured, it shows "unavailable" instead.
2. **`Viewer.updateAsk`** handles the keys while the panel is open. Typing appends to `a.input`, and up/down/tab select a suggestion. Enter picks the typed text or the selected suggestion and calls `startAnswer`.
3. **`Viewer.startAnswer`** makes a cancelable context and a channel. It starts a goroutine that calls `generate.Answer`, and returns `tea.Batch(waitFor(ch), answerTick())`. Progress arrives as `answerProgressMsg`, which updates a counter of files read. Esc cancels the context.
4. **`generate.Answer`** does the work:
   - `PrepareWorkspace` checks out the repo.
   - `writeLessons` and a `tour.json` dump of the current tour go into the workspace.
   - `askPrompt` builds the prompt.
   - `backend.Run` runs the LLM.
   - `settle` validates the result and repairs it if needed.
   - It returns the steps, with `Asked` set on the first one.
5. **`answerDoneMsg`** reaches `updateAsk`. It calls `Tour.InsertAfter(v.index, steps)`, refreshes `v.steps`, and jumps to the first new step with `goTo(v.index+1)`. It then shows the cost and saves the tour through `asker.Save`.

### diff-tour-ask/d064

Asking a question during a tour runs through these steps:

1. **Opening the panel.** `Viewer.openAsk` shows the question panel. It lists the step's suggested questions (`suggestions()`, from `Step.Asks`) plus a free-text line. If no `Asker` is configured, it shows "unavailable" instead.
2. **Typing.** `Viewer.updateAsk` handles `tea.KeyPressMsg` and `tea.PasteMsg`, collecting the text into `askState.input`. Pressing Enter sends either the highlighted suggestion or the typed text.
3. **Starting.** `Viewer.startAnswer` creates a cancelable context and a channel. It starts a goroutine that calls `generate.Answer`. It returns `waitFor(ch)` and `answerTick()` to drive the progress display.
4. **Generating.** `generate.Answer` runs `PrepareWorkspace`, `writeLessons` and `askPrompt`, and writes `tour.json` into the workspace. It then calls `backend.Run` and `settle`, which validates and repairs the result. It collects the steps, sets `steps[0].Asked` to the question, and returns the steps.
5. **Displaying.** The goroutine sends `answerDoneMsg`. `updateAsk` handles it by calling `Tour.InsertAfter`, refreshing `v.steps`, and calling `goTo(index+1)`. It then calls `asker.Save` to persist the tour.

### diff-tour-ask/ca1a

When you ask a question during a tour, the flow runs through `internal/ui/ask.go` and `internal/generate/ask.go`:

1. `Viewer.openAsk` shows the question panel. `Viewer.updateAsk` handles the keys: you pick one of the step's suggested `Asks` or type your own text, then press Enter.
2. `Viewer.startAnswer` switches the state to `askAnswering`. It starts a goroutine that calls `generate.Answer` and returns `waitFor(ch)` plus `answerTick` to the UI.
3. `generate.Answer` calls `PrepareWorkspace` to export the files and `writeLessons`, then writes `tour.json`. It builds the prompt with `askPrompt` and runs `backend.Run`. Next, `settle` validates the output and repairs it if needed. It then clears the `Question` and `Asks` fields on the new steps and sets `steps[0].Asked` to your question.
4. Progress events come back as `answerProgressMsg` for the "reading" display. The final result arrives as `answerDoneMsg`.
5. On success, `updateAsk` calls `Tour.InsertAfter`, refreshes `v.steps` and calls `goTo(index+1)`. It then calls `asker.Save` if one is set. On failure it shows a message instead.

### diff-tour-ask/05c0

Pressing `?` in `Viewer.Update` calls `openAsk`. `updateAsk` then handles typing and choosing a suggested question or free text. On Enter it calls `startAnswer`, which runs `generate.Answer` in a goroutine. It returns `waitFor` and `answerTick`, which keep the spinner panel (`askPanel`) updating.

`generate.Answer` does the following:
- It sets up the workspace with `PrepareWorkspace` and `writeLessons`.
- It writes the tour as `tour.json`.
- It builds the prompt with `askPrompt`.
- It calls `backend.Run` (for example `ClaudeCode.Run`).

`settle` and `parse` validate the reply, send a repair prompt to the backend if there are problems, and retry up to `MaxRepairs`. `Answer` then flattens the chapters into steps and marks the first one with `Asked`.

It sends `answerDoneMsg` back to `updateAsk`. That handler calls `Tour.InsertAfter` to put the steps after the current one, refreshes `v.steps`, calls `goTo(index+1)` to show the first new step, and calls `asker.Save` if one is set.

The Explore agent did not read the body of `parse` or the backends' output streaming.

### diff-tour-ask/3614

Pressing `?` or `/` in the viewer (`Viewer.Update`) calls `openAsk`, which switches to `askChoosing` mode. `updateAsk` then collects keystrokes into a rune buffer, or lets you pick a suggested question. Enter calls `startAnswer`.

`startAnswer` switches to `askAnswering` and runs `generate.Answer` in a goroutine. The UI listens on a channel through `waitFor`, and `answerTick` updates the elapsed time.

`Answer` does the following:
- Prepares the workspace.
- Builds the prompt with `askPrompt`.
- Calls `backend.Run`.
- Passes the reply to `settle`, which parses it and retries with `repairPrompt` if the reply is invalid.
- Flattens the returned chapters into steps.

The goroutine then sends `answerDoneMsg`. `updateAsk` handles it by:
- Calling `Tour.InsertAfter` to splice the new steps in after the current one.
- Calling `goTo` to jump to the first new step.
- Saving the tour with `SaveCached`.

### diff-tour-ask/6b28

Asking a question runs through `internal/ui/ask.go` and `internal/generate/ask.go`.

1. **Opening the panel.** `openAsk` sets `askChoosing` mode. If there's no `Asker`, it shows an "unavailable" message instead.
2. **Typing.** `updateAsk` handles key and paste messages. You can type free text or pick one of the step's suggested `Asks`. Enter then calls `startAnswer(q)`.
3. **Starting the answer.** `startAnswer` switches to `askAnswering` mode and creates a cancellable context. It starts a goroutine that calls `generate.Answer`. Progress and the final `answerDoneMsg` come back over a channel through `waitFor`, and `answerTick` drives the elapsed-time display.
4. **Generating the steps.** `generate.Answer` does the following:
   - `PrepareWorkspace` exports the files, and `writeLessons` and `tour.json` are written alongside them.
   - `askPrompt` builds the prompt.
   - `backend.Run` runs the LLM.
   - `settle` validates the output and repairs it if needed.
   - It returns one or two `tour.Step`s, with `Asked` set on the first.
5. **Showing the steps.** On `answerDoneMsg`, `updateAsk` calls `Tour.InsertAfter`, refreshes `v.steps` and calls `goTo(index+1)`. It then calls `asker.Save` if one is set.

### diff-tour-ask/6f7e

Pressing `?` or `/` in `Viewer.Update` (`viewer.go`) calls `openAsk`, which opens the panel. `updateAsk` (`ui/ask.go`) collects either a suggested question from the step's `Asks` or typed text. On Enter it calls `startAnswer`, which switches to the answering state and runs `generate.Answer` in a cancelable goroutine.

`Answer` (`generate/ask.go`) does the following:
1. `PrepareWorkspace` checks out the base and head files.
2. It writes the tour as `tour.json`.
3. `askPrompt` builds the prompt from the step list, the current step and the question.
4. `backend.Run` calls the LLM.
5. `settle` takes the reply.

`settle` loops up to `MaxRepairs`. Each pass calls `parse`, which runs `extractJSON`, `Tour.Validate`, `DropRevealingAsks` and `render.Check`. On problems it sends `repairPrompt` and parses the new reply.

Progress arrives as `answerProgressMsg`. When the call finishes, `answerDoneMsg` is handled. It calls `Tour.InsertAfter`, refreshes `Tour.Steps` and jumps to the first new step with `goTo`. `generate.SaveCached` then persists the tour, and `render.Frame` shows the original question above the new steps.

### diff-tour-ask/324c

Pressing `?` or `/` in `Viewer.Update` calls `openAsk`, and `askPanel` then draws the question box. It lists the step's suggested questions and a free-text input.

`updateAsk` handles the typing: printable keys and paste append to the input, `up`/`down`/`tab` move the selection, and `esc` closes the panel. `enter` calls `startAnswer`.

`startAnswer` switches to the answering state and launches a goroutine running `generate.Answer`. `waitFor` and `answerTick` feed its progress into the Bubble Tea loop and drive the spinner.

`generate.Answer` writes the current tour into the work dir. It builds the prompt with `askPrompt`, which fills `prompts/ask.md` and adds the question. It then runs `backend.Run` and checks the result with `settle`. It returns the new steps in an `answerDoneMsg`.

When `updateAsk` gets `answerDoneMsg`, it calls `Tour.InsertAfter(index, steps)`, refreshes `v.steps`, and calls `goTo(index+1)` to show the first new step. It then calls `asker.Save`, which writes the tour to the cache through `generate.SaveCached`.
