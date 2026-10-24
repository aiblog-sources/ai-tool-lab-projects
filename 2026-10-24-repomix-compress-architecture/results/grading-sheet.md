## ky-retry-flow

Trace what happens inside ky when a GET request gets a 503 response and retries are enabled. Name the internal methods involved, in order, and say when the beforeRetry hooks run.

Points:
1. The request goes run -> retry -> fetch (the private #run, #retry and #fetch methods, or equivalent names)
2. afterResponse hooks run on the 503 response before ky decides to throw or retry
3. A HTTPError is created and the retry decision (#retryFromError / #calculateRetryDelay) checks the retry limit, the method and the status code list
4. The delay comes from the Retry-After style header for statuses in afterStatusCodes (capped by maxRetryAfter), otherwise from retry.delay (0.3 s doubling) with optional jitter, capped by backoffLimit
5. ky waits for the delay first; beforeRetry hooks run after the delay and before the next fetch, then the retry count goes up and fetch runs again

### ky-retry-flow/2c7e

Flow (`source/core/Ky.ts`):

1. `Ky.create` → `#run()` awaits a microtask, then `#runBeforeRequestHooks()`, then `#retry()`.
2. `#retry()` calls `#fetch()`, which clones the request, calls `fetch` (via `timeout()` if a timeout applies) and returns the 503 `Response`.
3. Back in `#run()`'s loop: `#runAfterResponseHooks()`, then since `!ok` and `#shouldThrowHttpErrors()` is true, it builds an `HTTPError` and fills `data` via `#getResponseData()`.
4. `#retryFromError(httpError)` calls `#calculateRetryDelay()`. That checks the retry limit and method, runs `shouldRetry` if set, and checks `statusCodes`. 503 is in `afterStatusCodes`, so `getRetryTimingHeader`/`calculateRetryTimingDelay` are used if headers exist; otherwise `#calculateDelay()`.
5. `delay()` waits.
6. The `beforeRetry` hooks run in `#retryFromError`, after the delay and before the retry. Then `#retryCount++` and `#retry()` runs again.

The loop in `#run()` repeats until success or a thrown error, which goes through `#throwProcessedError()`.

### ky-retry-flow/ad53

Flow in `Ky`:

1. `Ky.create` runs `init` hooks, builds `Ky`, and calls `#run()`.
2. `#run` runs `#runBeforeRequestHooks`, then `#retry()`, which calls `#fetch()`. A 503 response is returned, not thrown.
3. Back in `#run`'s loop, `#runAfterResponseHooks` runs. The response is not `ok`, so `#shouldThrowHttpErrors` is true. An `HTTPError` is built, `#getResponseData` fills `error.data`, and `retryError = httpError`.
4. `#retryFromError(retryError)` is called. It calls `#calculateRetryDelay`, which checks the retry limit and method, runs `shouldRetry`, and confirms the status is in `statusCodes`. For 503 (in `afterStatusCodes`), it uses `getRetryTimingHeader` and `calculateRetryTimingDelay`, otherwise `#calculateDelay`.
5. `#retryFromError` then waits via `delay()`.
6. **The `beforeRetry` hooks run after the delay**, inside `#retryFromError`, via `#raceWithTotalTimeout`. They run before `this.#retryCount++`.
7. Finally `#retry()` is called again, which calls `#fetch()`.

### ky-retry-flow/b1a1

Flow for a GET that gets a 503 (from memory of `source/core/Ky.ts`; I haven't checked it against current main):

1. `Ky.create()` builds the instance. GET is in `retry.methods`, so it wraps the request in `ky._retry(function_)`.
2. `function_` awaits `ky._fetch()`, which clones the request and calls fetch. `afterResponse` hooks then run, followed by `_decorateResponse()`.
3. The response is not ok and `throwHttpErrors` is set, so it builds an `HTTPError`. `beforeError` hooks run, then it throws.
4. `_retry` catches the error and calls `_calculateRetryDelay(error)`. That checks the retry limit, the status code (503 is in `retry.statusCodes`), and `Retry-After`. It then increments `_retryCount` and returns the delay in ms. If the error is not retriable, it rethrows instead.
5. `_retry` awaits `delay(ms)`.
6. The `beforeRetry` hooks run after the delay and before the next attempt. They receive `request`, `options`, `error` and `retryCount`. A hook can return the `stop` symbol to end retrying.
7. `_retry(function_)` recurses, so `_fetch()` runs again.

### ky-retry-flow/cc01

The packed file strips method bodies, so I can only give the order the comments and signatures support.

1. `Ky.create` runs the constructor, then `#run` and `#fetch` send the request.
2. `#runAfterResponseHooks` runs on the response.
3. For the 503, `#shouldThrowHttpErrors` is presumably consulted, and an `HTTPError` is built with `#getResponseData` populating `error.data`. The bodies that would confirm this are missing.
4. `#retry` and `#retryFromError(error)` take over. `#calculateRetryDelay` decides eligibility (method, `shouldRetry`, status). For 503, `getRetryTimingHeader` and `calculateRetryTimingDelay` supply server timing, and otherwise `#calculateDelay` applies backoff.
5. In `#retryFromError`, the comments show it checks the remaining `totalTimeout` budget and waits via `delay`. It then resets the abort controller and applies any forced-retry request.
6. **The `beforeRetry` hooks run after that delay**, in a loop inside `#retryFromError`. A returned `{options}` goes to `#applyBeforeRetryOptions`, and `ky.stop` ends the retry.
7. The next `#fetch` call is not visible here, because the method bodies are missing.

### ky-retry-flow/51d0

The packed file strips function bodies, so I can only give the order as far as names and comments show it.

1. `Ky.create` / the constructor builds the request, and `#run()` starts the pipeline.
2. `#runBeforeRequestHooks()` runs, then `#fetch()` sends the request.
3. `#runAfterResponseHooks()` runs.
4. For a non-OK status, `#shouldThrowHttpErrors()` is checked and an `HTTPError` is produced. `#getResponseData()` fills `error.data`. The exact call sequence is not shown.
5. `#retry()` calls `#retryFromError()`, which uses `#calculateRetryDelay()`. That checks method, limit and `shouldRetry`, and reads headers via `getRetryTimingHeader`/`calculateRetryTimingDelay` (503 is in `afterStatusCodes`). `#calculateDelay()` handles the backoff.
6. The `delay()` utility waits.
7. The abort controller is reset, and then the `beforeRetry` hooks run, after the wait and before the next `#fetch()`.

Missing: the actual bodies, so the exact call chain and hook loop are not visible.

### ky-retry-flow/5935

Flow in `source/core/Ky.ts`, on a 503 GET:

1. `Ky.create()` builds the instance and defines an inner `function_`. GET is in `retry.methods`, so it runs `ky._retry(function_)`.
2. `function_` calls `_fetch()`. This runs the `beforeRequest` hooks, then the underlying `fetch`.
3. The `afterResponse` hooks run, then `_decorateResponse()`.
4. Because `!response.ok` and `throwHttpErrors` is set, an `HTTPError` is created. The `beforeError` hooks run and it is thrown.
5. The `catch` in `_retry()` calls `_calculateRetryDelay(error)`. This increments `_retryCount` and checks the limit, the status code (503 is in `retry.statusCodes`), and any `Retry-After` header. Otherwise it uses the backoff delay. If retries are exhausted, `_retry()` rethrows the error.
6. `_retry()` waits for the delay.
7. The `beforeRetry` hooks run after the delay and before the next attempt. They receive `{request, options, error, retryCount}`, and a hook can return `ky.stop` to abort the retry.
8. `_retry(function_)` recurses, which repeats from step 2.

I'm recalling this from memory and haven't checked it against current main, so details such as the newer `shouldRetry` option may differ.

### ky-retry-flow/f216

The repo is compressed, so method bodies are stripped. I can only infer the order from names and comments, and the exact internal retry method names are missing.

1. `Ky.create` → constructor builds the request and options. The `init` and `beforeRequest` hooks run.
2. `#fetch()` sends the request through `timeout()`. Afterwards it clones the request for a possible retry.
3. `afterResponse` hooks run on the response.
4. A 503 is non-ok, so an `HTTPError` is built, and `#getResponseData` fills `error.data`.
5. The retry decision uses `normalizeRetryOptions` settings (limit, methods, statusCodes, `shouldRetry`). 503 is in the default `afterStatusCodes`, so `getRetryTimingHeader` and `calculateRetryTimingDelay` pick the delay. Otherwise the `retry.delay` backoff applies.
6. `delay()` waits.
7. `beforeRetry` hooks run after the wait, with `retryCount ≥ 1`, and before the next `#fetch()`.

If retries are exhausted, `#throwProcessedError` runs `beforeError`.

I can't confirm the exact loop or method names for steps 4–6, or the relative order of steps 3 and 4.

### ky-retry-flow/de98

The compressed file strips every method body, so I can only give the order that the comments and imports support.

1. `Ky.create` builds the instance. Its constructor normalizes the options and the request.
2. `#fetch()` clones the request in advance for retries and calls the `timeout` util, which calls the user `fetch`.
3. The response goes through `afterResponse` hooks. A non-2xx status then makes Ky create an `HTTPError`, populated via `#getResponseData`.
4. The retry decision is made. It uses the retry limit, the method check, `shouldRetry`, and `statusCodes` (503 is in the default list).
5. 503 is in the default `afterStatusCodes`, so `getRetryTimingHeader` and `calculateRetryTimingDelay` pick the delay. Otherwise the `retry.delay` backoff applies.
6. The `delay` util waits, after a check against the `totalTimeout` budget.
7. The abort controller is reset, and any forced-retry request is applied.
8. The `beforeRetry` hooks run, after the wait and before the next `#fetch()` attempt.

Missing: the retry method's name and the exact control flow, because those bodies are stripped.

## ky-timeout

How does ky enforce the `timeout` option on a single attempt, and what happens to the timeout when the request is retried?

Points:
1. A timeout() helper races fetch against a setTimeout timer
2. When the timer fires it aborts ky's AbortController and rejects with a TimeoutError
3. Each attempt gets its own timeout (the timer is cleared in finally), limited by the remaining totalTimeout budget when one is set
4. TimeoutError is only retried when retryOnTimeout is enabled (or shouldRetry returns true); by default it is not
5. Before a retry after a timeout, the aborted AbortController is replaced with a fresh one (new managed signal) so the next attempt is not born aborted

### ky-timeout/273f

**Single attempt:** `Ky._fetch()` calls the `timeout()` function in `source/utils/timeout.ts` (unless `timeout === false`). `timeout()` races the `fetch` promise against a `setTimeout`. When the timer fires, it calls `abortController.abort()` to cancel the request and rejects with `TimeoutError`. When the fetch settles first, it clears the timer with `clearTimeout`.

**On retry:** The retry logic (`Ky#_retry`, which wraps `_fetch`) calls `_fetch()` again. Each attempt therefore goes through `timeout()` again and gets a fresh timer for the full `timeout` value. The timeout is per attempt, not cumulative, and the backoff delay between attempts doesn't count toward it.

A `TimeoutError` is also not retried by default. It is retried only if `retry.retryOnTimeout` is set to `true`.

I'm working from memory of the code, not from files in this directory, so details may differ slightly on current main.

### ky-timeout/1626

In `source/utils/timeout.ts`, `timeout()` wraps the `options.fetch` call in a Promise. It starts a `setTimeout` that rejects with `TimeoutError`, and it aborts the request's `AbortController` if one exists. The timer is cleared with `clearTimeout` once fetch settles. `Ky._fetch()` calls `timeout()` unless `timeout` is `false`.

On retry, `Ky` (in `Ky.ts`) uses `_retry()`, which waits the computed retry delay and then calls `_fetch()` again. That call invokes `timeout()` again with a fresh timer and a fresh abort setup, so each attempt gets the full `timeout` value. The delay between attempts isn't counted. A `TimeoutError` can itself trigger a retry, but this depends on the `retry` options, such as `retry.limit`.

I'm working from memory and haven't checked the current main branch, so details may differ.

### ky-timeout/d6bd

**Enforcement:** In `Ky#fetch()`, `#getEffectiveTimeout()` takes the smaller of `options.timeout` and the remaining `totalTimeout` budget (from `#getRemainingTotalTimeout()`). If that is `undefined` (no limits), it calls `fetch` directly. Otherwise it calls `timeout()` (utils/timeout.ts), which starts a `setTimeout`. When the timer fires, it calls `abortController.abort()` and rejects with `TimeoutError`. A `finally` clears the timer, and a late response is discarded via `cancelBody`.

**On retry:** Each attempt goes through `#fetch()` again (via `#retry()` / `#retryFromError()`), so `#getEffectiveTimeout()` is recomputed and a fresh timer starts. `timeout` is therefore per-attempt. In `#retryFromError()`, if the abort controller was aborted by the timeout, a new `AbortController` and managed signal are created and the request is re-signalled with `#withManagedSignal`. Whether a timeout is retried at all depends on `retryOnTimeout` in `#calculateRetryDelay()`, which defaults to `false`.

### ky-timeout/1efd

**Enforcement:** The default export of `source/utils/timeout.ts` (`timeout(request, init, abortController, options)`) wraps the fetch call (the user-supplied `options.fetch`, called unbound) in a `Promise.race()` workaround (#91) against a timer. When the timer wins, the request is aborted via the `AbortController`, and it rejects with a `TimeoutError`. A `finally` clears the timer, even if fetch throws synchronously. A response arriving after the timeout already won is discarded and its body cancelled via `cancelBody`.

**On retry:** `Ky.#fetch()` runs for each attempt, so `timeout` is applied independently per attempt (the docs say "applied independently to each retry"). In `#retryFromError`, if the `abortController` was aborted (as happens on timeout), it is reset so the retried request gets a fresh signal; `#createManagedSignal` is the related signal helper. Whether a timeout is retried is controlled by `retry.retryOnTimeout`, handled in `#calculateRetryDelay`.

**Not shown:** the compressed body of `timeout.ts` (the timer and abort lines) and the exact `#fetch()` call that passes `timeout` to it are elided in the packed content, so I can't show those lines directly.

### ky-timeout/4809

**Single attempt:** The `timeout()` function in `utils/timeout.ts` enforces it. Its comment calls it a `Promise.race()` workaround (#91). It calls the user's `fetch` unbound, and the code shows that a `finally` clears the timer. If the timeout wins, it rejects with `TimeoutError`. A response that arrives late is discarded and its body is cancelled via `cancelBody`. `validateTimeoutOption` checks the value, and the `maxSafeTimeout` constant caps it.

**On retry:** The `timeout` option docs say it applies independently to each attempt. Because the timer is cleared in `finally`, it cannot abort a later attempt. `Ky.ts` also resets the `abortController` if a timeout aborted it, so the retried request gets a fresh signal. The retry only happens when `retry.retryOnTimeout` is true. `totalTimeout` is a separate overall budget.

**Not shown:** The compression removed the body of `timeout()`, including the actual `setTimeout`/abort logic. The exact call site in `Ky.ts` is also not visible.

### ky-timeout/9efc

**Enforcement:** The per-attempt timeout lives in `timeout()` in `utils/timeout.ts`. Its signature is `(request, init, abortController, options)`, and its comment calls it a `Promise.race()` workaround (#91). It calls `options.fetch` unbound and imports `TimeoutError`. The comments say a `finally` clears the timer so it can't abort a later attempt. A response that arrives after the timeout has won is discarded, and `cancelBody` releases its body. `validateTimeoutOption` in `Ky.ts` checks the option value.

**On retry:** The `timeout` docs say it applies independently to each retry. A timeout is only retried if `retry.retryOnTimeout` is true (default false). `Ky.ts` has a comment that the `abortController` is reset if it was aborted, which happens on a timeout retry, so the retried request gets a fresh signal.

**Not shown:** The body of `timeout()`, where the timer is set and the abort or rejection happens, because the code is compressed. The exact call site in `#fetch` is also not shown.

### ky-timeout/530e

**Enforcing it:** In `Ky.#fetch()`, `#getEffectiveTimeout()` returns the smaller of `options.timeout` and the remaining `totalTimeout` budget, or `undefined` if neither applies. With `undefined`, `fetch` is called directly. Otherwise `#fetch()` calls `timeout()` (utils/timeout.ts). It sets a `setTimeout`. On expiry it calls `abortController.abort()` and rejects with `TimeoutError`. A late response is discarded via `cancelBody`, and `finally` clears the timer.

**On retry:** `#retryFromError()` ends by calling `#retry()`, which calls `#fetch()` again. That recomputes the effective timeout, so each attempt gets a fresh `timeout`. If the abort controller was aborted by a timeout, `#retryFromError()` creates a new `AbortController` and managed signal, then reassigns the request via `#assignRequest(#withManagedSignal(...))`. Timeouts are retried only if `retry.retryOnTimeout` is true (checked in `#calculateRetryDelay`). The `beforeRetry` hook can change `timeout` via `#applyBeforeRetryOptions`.

### ky-timeout/4412

**Per-attempt enforcement:** The default export of `utils/timeout.ts`, `timeout(request, init, abortController, options)`, enforces it. Comments describe it as a `Promise.race()` workaround (#91). The fetch races against a timer that rejects with `TimeoutError`. The `finally` clears the timer, so it can't abort a later retry. A response that arrives after the timeout has won gets its body cancelled via `cancelBody`. `Ky.#fetch` calls it with the unbound `fetch` from `options`.

**On retry:** The `timeout` option is documented as applying independently to each attempt. A timeout is only retried if `retry.retryOnTimeout` is set, which `#calculateRetryDelay` checks. In `#retryFromError`, an aborted `abortController` is reset ("happens on timeout retry") so the next attempt gets a fresh signal. `#applyBeforeRetryOptions` lets a `beforeRetry` hook change `timeout`. `#getEffectiveTimeout` takes the smaller of `timeout` and the remaining `totalTimeout`.

**Missing:** The function bodies are compressed out. The exact timer and `abortController.abort()` code is not shown.

## ky-hooks-order

In what order do ky's hooks run during one request that keeps failing with an HTTP error until the retries run out, and which hooks can replace the response?

Points:
1. init hooks run first, synchronously, on the options before the request is built
2. beforeRequest hooks run once before the first fetch; returning a Response skips the fetch (and that response is not retried)
3. afterResponse hooks run after every response, including each retry; they can return a new Response or force a retry with ky.retry()
4. beforeRetry hooks run before each retry attempt; they can return a Request, a Response, or ky.stop
5. beforeError hooks run last, once, when the final HTTPError is thrown; they can return a modified error

### ky-hooks-order/a25d

**Order per attempt** (`Ky` class in `source/core/Ky.ts`):

1. `#fetch()` runs the `beforeRequest` hooks, then calls the `fetch` option.
2. Back in the function passed to `#retry()`, the `afterResponse` hooks run on a cloned response (decorated with `#decorateResponse`).
3. If `!response.ok` and `throwHttpErrors` is set, an `HTTPError` is created and the `beforeError` hooks run on it.
4. `#retry()` catches the error and checks the retry limit, `shouldRetry` and the delay from `calculateRetryDelay`. If a retry is allowed, the `beforeRetry` hooks run, then the loop returns to step 1.

When retries run out, the last `HTTPError` (after `beforeError`) is thrown. `beforeRetry` does not run after the final attempt.

**Hooks that can replace the response:**
- `beforeRequest`: returning a `Response` skips the network call.
- `afterResponse`: returning a `Response` replaces the current one, and this is the usual way to do it.
- `beforeRetry`: in recent versions, returning a `Response` short-circuits the retry. A `Request` return only replaces the request, and `ky.stop` aborts retries.

`beforeError` can only replace the error, not the response.

I'm working from memory and haven't checked the current source, so the exact placement of `beforeError` and the `beforeRetry` response return may differ on main.

### ky-hooks-order/ea53

**Order (inferred from method names and hook docs; Ky.ts bodies are compressed away, so the exact loop code isn't shown):**

1. `init` hooks run synchronously in the `Ky` constructor, on the options copy from `cloneInitHookOptions`.
2. `#runBeforeRequestHooks` runs once, with `retryCount` 0.
3. `#fetch` sends the request, then `#runAfterResponseHooks` runs.
4. A failing status raises an `HTTPError`. `#calculateRetryDelay` and `#retryFromError` then decide whether to retry.
5. `#retryFromError` runs the `beforeRetry` hooks, with `retryCount` ≥ 1.
6. Steps 3–5 repeat, running `afterResponse` after each attempt, until the retries run out.
7. `#throwProcessedError` runs the `beforeError` hooks and throws the final error.

**Hooks that can replace the response:**
- `beforeRequest` can return a `Response`, which skips the fetch.
- `afterResponse` can return a `Response`.
- `beforeRetry` can return a `Response`, which skips the retry.

`beforeError` can only replace the error, and `init` can't replace anything.

**Not shown:** the exact sequencing code inside `#run` and `#retry`.

### ky-hooks-order/1cd8

**Order (inferred from the hook docs and surviving comments in `Ky.ts`; most function bodies are stripped):**

1. `init` hooks run synchronously in the `Ky` constructor, on the `cloneInitHookOptions` copy.
2. `beforeRequest` hooks run once, with `retryCount` 0.
3. Each attempt calls `#fetch()`, then the `afterResponse` hooks run (`retryCount` 0, then 1, 2, and so on).
4. A failing status throws `HTTPError`, with `error.data` populated first. `shouldRetry` and the retry-timing helpers (`calculateRetryTimingDelay`) decide whether to retry.
5. `beforeRetry` hooks run (`retryCount` ≥ 1), then the loop returns to step 3.
6. Once retries are exhausted, `beforeError` hooks run and the error is thrown.

**Hooks that can replace the response:**
- `beforeRequest`: returning a `Response` skips the fetch and the remaining `beforeRequest` hooks.
- `afterResponse`: returning a `Response` overrides it.
- `beforeRetry`: returning a `Response` skips the retry.

A `Response` returned from `beforeRequest` or `beforeRetry` is used as-is, so a failing status throws rather than retries.

**Not shown:** the exact placement of the status check relative to `afterResponse`, since the loop body is compressed away.

### ky-hooks-order/12bd

**Order for a request that keeps failing with an HTTP error** (the function bodies are compressed away, so this comes from the names, comments and docs):

1. `init` hooks run synchronously while the `Ky` constructor builds the options.
2. `#run` calls `#runBeforeRequestHooks` once, with `retryCount` 0.
3. `#fetch` sends the request.
4. `#runAfterResponseHooks` runs on the response.
5. A non-2xx status becomes an `HTTPError`, and `#getResponseData` fills `error.data`. The exact point where the status check happens relative to step 4 isn't visible in the compressed code.
6. `#calculateRetryDelay` runs `shouldRetry` and works out the delay.
7. `#retryFromError` waits, then runs the `beforeRetry` hooks, then `#fetch` again. Steps 4 to 7 repeat until `retry.limit` is used up.
8. `#throwProcessedError` runs the `beforeError` hooks and throws.

**Hooks that can replace the response:**
- `afterResponse` by returning a `Response`.
- `beforeRequest` by returning a `Response`, which skips the HTTP request.
- `beforeRetry` by returning a `Response`, which skips the retry.

`beforeRetry` can also return `ky.stop`, which ends the request with no response at all.

### ky-hooks-order/ce65

Order (from `Ky.create`, `#run`, `#retryFromError`):

1. `init` hooks run synchronously in `Ky.create`, before construction.
2. `#run` calls `#runBeforeRequestHooks` once (`retryCount` 0).
3. `#retry` → `#fetch` sends the request.
4. `#runAfterResponseHooks` runs each `afterResponse` hook.
5. A non-ok status makes `#run` build an `HTTPError` and populate `data` via `#getResponseData`, then `#retryFromError`.
6. `#calculateRetryDelay` checks limits, `shouldRetry`, and status codes, then the delay runs.
7. `beforeRetry` hooks run, then `#retry` repeats steps 3–7.
8. When `#calculateRetryDelay` finds the limit reached, it throws the error, and `#throwProcessedError` runs `beforeError` hooks before the final throw.

Response replacement:
- `beforeRequest` can return a `Response`, skipping the fetch.
- `afterResponse` can return a `Response`.
- `beforeRetry` can return a `Response`, which skips the retry.

`beforeRequest` and `beforeRetry` can also return a replacement `Request`.

### ky-hooks-order/6399

**Order (from the hook docs and comments, since the function bodies in `Ky.ts` are stripped):**

1. `init` hooks run synchronously in the `Ky` constructor, on the copies made by `cloneInitHookOptions`.
2. `beforeRequest` runs once, with `retryCount` 0.
3. For each attempt, `#fetch()` sends the request, then `afterResponse` runs. A failing status then becomes an `HTTPError`. The exact order of the `afterResponse` and status check isn't shown.
4. If retries remain, `shouldRetry` decides, then `beforeRetry` runs (`retryCount` ≥ 1). The loop then repeats from step 3.
5. When retries are exhausted, `#throwProcessedError` runs `beforeError`, after `error.data` is populated.

**Hooks that can replace the response:**
- `beforeRequest` can return a `Response`, which skips the fetch and the remaining `beforeRequest` hooks.
- `afterResponse` can return a `Response`, and can also return `ky.retry()` to force a retry.
- `beforeRetry` can return a `Response`, which skips the retry.

`beforeError` only replaces the error, not the response.

**Missing:** the actual control flow in `Ky.ts` is not visible, so the finer ordering is inferred from comments.

### ky-hooks-order/2968

Hook order, as I recall it from `source/core/Ky.ts` (not re-checked against current main):

1. `Ky.create` wraps the attempt function in `#retry`.
2. Each attempt runs `#fetch`, which runs the `beforeRequest` hooks and then calls `options.fetch`.
3. The `afterResponse` hooks run on the result. If the response is not ok and `throwHttpErrors` is set, an `HTTPError` is thrown.
4. `#retry` catches the error and decides whether to retry, using `#calculateRetryDelay`. If it retries, it runs the `beforeRetry` hooks, waits, and starts the next attempt at step 2.
5. Once retries are exhausted, the `beforeError` hooks run on the `HTTPError`, which is then thrown.

The hooks that can replace the response are:
- `beforeRequest`: returning a `Response` skips the fetch.
- `afterResponse`: returning a new `Response` replaces the current one.
- `beforeRetry`: returning a `Response` ends the retry loop with that response.

`beforeError` can only replace the error, not the response.

I'm less sure about the `beforeRequest` and `beforeRetry` details than about the overall order.

### ky-hooks-order/ff27

Order (Ky.ts):

1. `init` hooks run synchronously in `Ky.create()`.
2. `#run()` calls `#runBeforeRequestHooks()` once (retryCount 0).
3. `#retry()` → `#fetch()` sends the request.
4. `#runAfterResponseHooks()` runs each `afterResponse` hook on a cloned response.
5. On a non-ok status, `HTTPError` is built, `#getResponseData()` fills `error.data`, and `#retryFromError()` → `#calculateRetryDelay()` (which consults `shouldRetry`, though that is an option, not a hook) decides whether to retry.
6. After the delay, `beforeRetry` hooks run, then `#retry()` fetches again. Steps 3–6 repeat.
7. When `retry.limit` is reached, `#calculateRetryDelay()` throws the error, and `#throwProcessedError()` runs the `beforeError` hooks.

Hooks that can replace the response:
- `beforeRequest` can return a `Response`, which skips the fetch and the remaining `beforeRequest` hooks.
- `afterResponse` can return a `Response`.
- `beforeRetry` can return a `Response`, which skips the retry and the remaining `beforeRetry` hooks.

A response returned by `beforeRequest` or `beforeRetry` is used as-is, so a failing status throws instead of retrying.

## ky-json-shortcut

When I call `ky.get(url).json()`, how does the Accept header get set to application/json even though `.json()` is called after `ky.get()` already returned?

Points:
1. Ky.create returns a promise object with body method shortcuts (json, text, blob...) attached
2. The shortcut sets the accept header to the method's MIME type on the pending request, unless an Accept header is already set
3. The fetch is delayed by one microtask (#run starts with await Promise.resolve()) so the synchronous shortcut call runs before the request is sent
4. If the request was already sent, the header only changes later attempts (the shortcut checks request vs original request)
5. The json shortcut then reads the body as text and parses it with parseJson or JSON.parse (with optional schema validation)

### ky-json-shortcut/5f25

The request isn't sent synchronously when `ky.get()` returns.

- `Ky.create()` (static) builds a `Ky` instance and starts an async `result` promise. That promise first waits a microtask, then calls `ky._fetch()`, so the request is dispatched after the current synchronous code finishes.
- Still in `Ky.create()`, it loops over `responseTypes` (`json`, `text`, `blob`, and so on) and attaches a method to `result` for each one. The `.json()` method sets the header with `ky.request.headers.set('accept', ky.request.headers.get('accept') || mimeType)`, where `mimeType` is `application/json`. This only happens if no Accept header was already set.
- `.json()` runs synchronously right after `ky.get()` returns, before the deferred `_fetch()` runs. The header is therefore already on the `Request` when it's sent.
- `.json()` then awaits `result` and parses the body, returning `''` for a 204 response.

I'm working from memory and haven't checked this against the current main branch, so exact names may differ slightly.

### ky-json-shortcut/b4ad

The fetch is deliberately delayed, so the header can be set after `ky.get()` returns.

1. `Ky.create()` builds a `Ky` instance and starts an async IIFE that calls `ky.#run()`.
2. `#run()` begins with `await Promise.resolve()`. The comment says this delays the fetch "so that body method shortcuts can set the Accept header."
3. `Ky.create()` then attaches a shortcut such as `result.json` for each type from `getSupportedResponseTypes()`. The media type comes from the `responseTypes` map in constants.ts, which gives `application/json` for `json`.
4. When `.json()` is called synchronously after `ky.get()`, the shortcut checks `ky.request !== ky.#originalRequest`. If they differ, it runs `ky.request.headers.set('accept', ky.request.headers.get('accept') || mimeType)`, so an Accept header already set is kept. Only after that does it `await result`.

`#fetch()` runs after the microtask delay, so it sends the request with the header already set.

The repo doesn't show how `requestMethods` such as `get` reach `Ky.create()` beyond `createRequest` in index.ts.

### ky-json-shortcut/7d3a

The fetch is deferred, so `.json()` can still set the header before the request is sent. The repo shows this only through comments, because the function bodies are compressed out.

- The `Ky` constructor has the comment "Delay the fetch so that body method shortcuts can set the Accept header". The request is therefore not dispatched synchronously when `ky.get()` returns.
- `Ky.create()` builds the `ResponsePromise` and attaches the body-method shortcuts (`json`, `text`, and so on). `getSupportedResponseTypes()` decides which ones are exposed, and `.bytes()` only if the runtime supports it.
- `responseTypes` in `constants.ts` holds the media type each shortcut advertises, per its comment.
- A comment says that before dispatch `ky.request` is the request that will be sent. That is why a shortcut can change its headers.
- After dispatch, `#fetch()` swaps in a clone for retries, so a late shortcut only affects later attempts.

**Missing:** the code that actually calls `headers.set('Accept', …)` and the code that does the deferral (e.g. a microtask or promise) are not in the compressed content.

### ky-json-shortcut/069f

The request is deferred, not sent immediately. In `Ky.create()`, the returned promise gets body-method shortcuts (`.json()`, `.text()`, etc.) attached, built from `responseTypes` / `getSupportedResponseTypes()` in constants.ts. Each shortcut, when called, sets the `Accept` header on the request (the `ky.request`) to the media type listed in `responseTypes`, then reads the body.

This works because the actual fetch is delayed. A comment in `#run()` says: "Delay the fetch so that body method shortcuts can set the Accept header." So `#run()` waits before calling `#fetch()`, giving `.json()` time to modify `ky.request` first.

A comment also notes that once the request is dispatched, `#fetch()` replaces `ky.request` with a retry clone, so a late shortcut only affects later attempts.

Missing: the compressed source omits the bodies of `Ky.create()` and `#run()`, so the exact delay mechanism (e.g. microtask or timer) and the header-setting code aren't shown.

### ky-json-shortcut/1d28

The request isn't sent synchronously. `ky.get()` calls `Ky.create()`, which builds a `Ky` instance and starts an async IIFE (`result`). That IIFE does `await Promise.resolve()` before calling `ky._fetch()`. This defers the fetch by a microtask, so synchronous code after `ky.get()` can still change the request.

`Ky.create()` then attaches a method for each entry in `supportedResponseTypes` (`json`, `text`, `blob`, and so on) to the returned promise. When you call `.json()` synchronously, it runs:

```ts
ky.request.headers.set('accept', ky.request.headers.get('accept') || mimeTypes[type]);
```

This sets `Accept: application/json` unless you already set an Accept header. The request hasn't been fetched yet, so the header is included. The method then awaits `result` and parses the response, which is handled via `ky._decorateResponse` and `response.json()`.

If you `await` something before calling `.json()`, the fetch will already have fired. I'm working from memory of the source, not from the files.

### ky-json-shortcut/0173

The fetch is deliberately delayed, so `.json()` can still change the request before it's sent.

1. `ky.get()` calls `createRequest`, which calls `Ky.create()`. That starts an async IIFE running `ky.#run()` and returns the promise immediately.
2. `#run()` begins with `await Promise.resolve()` (commented "Delay the fetch so that body method shortcuts can set the Accept header"). Nothing is fetched until that microtask yields.
3. `Ky.create()` attaches shortcut methods (from `getSupportedResponseTypes()`) onto the result, using the `responseTypes` map in `constants.js`, where `json` maps to `'application/json'`.
4. When `.json()` is called synchronously after `ky.get()`, the shortcut checks `ky.request !== ky.#originalRequest`. That is true before dispatch, since `#originalRequest` is only set in `#fetch()`. It then runs `ky.request.headers.set('accept', ky.request.headers.get('accept') || mimeType)`.

So a user-supplied Accept header is preserved, and otherwise `application/json` is set. The shortcut then awaits the result and parses the text.

### ky-json-shortcut/d1d5

The fetch is deliberately delayed so the body-method shortcuts can set the header first.

- `Ky.create(input, options)` builds the `Ky` instance and returns a `ResponsePromise`. It attaches a method for each entry in `responseTypes` (`json`, `text`, `blob`, etc.), checked through `getSupportedResponseTypes()`.
- The constructor's comment says: "Delay the fetch so that body method shortcuts can set the Accept header". So the actual `#fetch()` call is scheduled asynchronously and doesn't run immediately when `ky.get()` returns.
- When `.json()` is called synchronously afterward, the shortcut sets the `Accept` header on `ky.request` before dispatch. Per the comments, before dispatch `ky.request` is the request that will be sent. After dispatch, a late shortcut only affects later attempts, never a request already sent.
- `responseTypes` in `constants.ts` holds the media type each shortcut advertises.

Not shown: the exact lines where the header is set (the code was compressed out), and the exact scheduling mechanism for the delay.

### ky-json-shortcut/2760

The mechanism is a deferred fetch. Per the comments in `Ky.create()` and `#run()`, "Delay the fetch so that body method shortcuts can set the Accept header."

- `Ky.create()` returns a `ResponsePromise` and decorates it with body-method shortcuts, built from `responseTypes` and `getSupportedResponseTypes()` in `constants.js`.
- The actual `fetch` is postponed inside `#run()`, so `ky.get(url)` has not dispatched yet when `.json()` is called.
- When `.json()` runs, its shortcut sets the `Accept` header on `ky.request` before dispatch. A comment says that before dispatch `ky.request` is the request that will be sent, and a late shortcut only affects later attempts.
- `responseTypes` holds the media type each shortcut advertises.

Missing: the shortcut's exact code, including the header-setting line and the delay mechanism in `#run()`, is not shown in the compressed content, so I can't confirm those details.

## ky-force-retry

How can an afterResponse hook force ky to retry a request, and how does a forced retry differ from a normal retry?

Points:
1. The hook returns ky.retry(...), which ky turns into a ForceRetryError
2. The run loop catches the ForceRetryError from the afterResponse hooks and goes to the retry path even for a successful response
3. A forced retry skips the retriable-method check, shouldRetry and the status code list
4. It still counts against the retry limit, and can carry a custom delay (used instead of the computed backoff)
5. It can carry a custom Request, which is applied before the beforeRetry hooks run

### ky-force-retry/589c

An `afterResponse` hook returns `ky.retry(options)` (the `retry` function in `constants.ts`). It builds a `RetryMarker` (recognized by `isRetryMarker`, a branded check that works across Ky copies). `ForceRetryError` is built from the marker's `ForceRetryOptions` (`delay`, `code`, `cause`, `request`), validating them and throwing a `TypeError` for bad values.

Differences from a normal retry:
- It works even on a successful status code, since it's triggered by the hook rather than by an error or status.
- It skips the method check and `shouldRetry`, but still respects `retry.limit`.
- The error seen in `beforeRetry` is a `ForceRetryError` (`isForceRetryError`), with message `Forced retry: <code>`.
- A custom `delay` bypasses jitter and `backoffLimit`.
- A custom `request` replaces the request (Ky re-attaches its managed signal), before `beforeRetry` hooks run.

Missing: the exact retry-loop code in `Ky.ts` is compressed away, so I can't show precisely where these checks occur.

### ky-force-retry/873f

An `afterResponse` hook forces a retry by returning `ky.retry(options?)`. This creates a `RetryMarker`. The options can include `delay`, `code`, and a replacement `request`.

In `Ky`'s response handling, the hook-running code sees the marker and throws a `ForceRetryError`. That error goes through the same retry path as other errors, via the internal `#retry` and `#calculateRetryDelay` logic. `beforeRetry` hooks still run.

**Differences from a normal retry:**
- **Eligibility:** A normal retry is allowed only if the error passes the `retry` checks: the method is in `methods`, the status is in `statusCodes`, and `shouldRetry` agrees. A `ForceRetryError` skips those checks.
- **Limit:** Both kinds are still bound by `retry.limit`.
- **Delay:** A forced retry uses the marker's custom `delay` if given. Otherwise it uses the normal backoff. A normal retry uses `Retry-After` headers or backoff, capped by `backoffLimit`.
- **Request:** A forced retry can swap in a custom `request`.

I'm working from memory of the source, not a fresh read, so check the exact ordering of these checks in `Ky.ts`.

### ky-force-retry/d0e1

An `afterResponse` hook can force a retry by returning `ky.retry()`, optionally with `{delay, code, request}`. This is from memory, not from reading the source in this environment.

- **Mechanism:** `ky.retry()` returns a `RetryMarker`. After running the hooks, Ky's internal `#runAfterResponseHooks` sees the marker and throws a `ForceRetryError`. The normal retry path (`#retry`/`#calculateRetryDelay`) catches that error.
- **Normal retry:** Ky retries after a thrown `HTTPError` or network error, but only if the method, status code and `retry.limit` allow it. Delay comes from the backoff or `Retry-After`, and `retry.shouldRetry` can veto it.
- **Forced retry:** The hook decides, so the method and status checks and `shouldRetry` are skipped. A custom `delay` replaces the computed backoff, and `code` is reported as the reason. It still goes through `beforeRetry` hooks and, I believe, still counts against the retry limit.

I'm not fully certain of the exact limit behavior.

### ky-force-retry/375e

**How to force a retry:** An `afterResponse` hook returns `ky.retry(options)`, which is the exported `retry` function in `constants.ts`. It returns a `RetryMarker`, which `isRetryMarker` recognises even across duplicated Ky copies. Options are `delay`, `code`, `cause` and `request`. Ky builds a `ForceRetryError` (validating the options, wrapping non-Error causes in `NonError`) and routes it into the retry logic.

**Differences from a normal retry:**
- It works even on a successful status code, e.g. based on body content.
- It skips the method check and `shouldRetry`, but still respects `retry.limit`.
- A custom `delay` bypasses jitter and `backoffLimit`.
- A custom `request` replaces the outgoing request, with Ky's managed signal reattached, before `beforeRetry` hooks run.
- It is observable in `beforeRetry` and `beforeError` hooks as a `ForceRetryError`, detectable with `isForceRetryError`.

**Missing:** The `Ky.ts` code that actually handles the marker is compressed away, so I can't show the exact control flow.

### ky-force-retry/77fc

**Forcing a retry:** An `afterResponse` hook returns `ky.retry(options)`, which calls `retry()` and creates a `RetryMarker`. In `#runAfterResponseHooks`, `isRetryMarker` detects it and throws a `ForceRetryError` (validated in its constructor: `delay`, `code`, `cause`, `request`). `#run` catches it via `isForceRetryError` and calls `#retryFromError`.

**Differences from a normal retry** (in `#calculateRetryDelay`):
- A forced retry skips the method check (`retry.methods`) and `shouldRetry`; it still respects `retry.limit`.
- Delay is `customDelay` if given (bypassing jitter and `backoffLimit`), otherwise `#calculateDelay`.
- A `customRequest` is applied via `#assignRequest(#withManagedSignal(...))` before `beforeRetry` hooks.
- It works even on successful responses, and `beforeRetry` hooks see a `ForceRetryError`.

Normal retries are triggered by HTTP status, network, or timeout errors, and checked against status codes, `retryOnTimeout`, and the like.

### ky-force-retry/990c

An `afterResponse` hook returns `ky.retry(options)` (the `retry` function in `constants.ts`). It builds a `RetryMarker` (recognised by `isRetryMarker`, branded so it works across Ky copies). Options: `delay`, `code`, `cause`, `request`.

Differences from a normal retry:
- `#calculateRetryDelay` handles the forced case first: it skips the method check and `shouldRetry`, but still respects `retry.limit`.
- The error is a `ForceRetryError` (validated in its constructor; message is `Forced retry: <code>`), observable in `beforeRetry`/`beforeError` via `isForceRetryError`.
- A custom `delay` bypasses jitter and `backoffLimit`.
- A custom `request` is applied in `#retryFromError` before `beforeRetry` hooks, with Ky's managed signal re-attached and no header sanitizing.

Not shown: the exact code in `#runAfterResponseHooks` or `#calculateRetryDelay` that detects the marker and creates the `ForceRetryError` (the bodies are compressed out).

### ky-force-retry/5efe

An `afterResponse` hook returns `ky.retry(options)` (the exported `retry` in `constants.ts`). It builds a `RetryMarker` (recognized by `isRetryMarker`, via a brand so duplicate Ky copies work). Options (`delay`, `code`, `cause`, `request`) are validated by `ForceRetryError`'s constructor. `#runAfterResponseHooks` sees the marker, and the retry flows through `#retry`/`#retryFromError`, surfacing a `ForceRetryError` to `beforeRetry` hooks (detectable with `isForceRetryError`).

Differences from a normal retry (`#calculateRetryDelay`):
- It skips the method-retriable check and `shouldRetry`.
- It still respects `retry.limit`.
- A custom `delay` bypasses jitter and `backoffLimit`.
- A custom `request` replaces the request (used as-is, with Ky's managed signal reattached) before `beforeRetry` hooks run.
- It works on successful status codes.

Not shown: the exact body of `#runAfterResponseHooks` and `#calculateRetryDelay` where the marker is converted, since the compressed source omits most method bodies.

### ky-force-retry/7a33

An `afterResponse` hook returns `ky.retry(options)` (the `retry` function in constants.ts), which creates a `RetryMarker`. In `#runAfterResponseHooks`, `isRetryMarker` detects it and throws a `ForceRetryError` built from the marker's options (`delay`, `code`, `cause`, `request`). `#run` catches it via `isForceRetryError` and passes it to `#retryFromError`.

Differences from a normal retry, in `#calculateRetryDelay`:
- It still respects the `retry.limit` check.
- It skips the method check (`retry.methods`) and `shouldRetry`.
- It skips status-code, timeout, and network-error eligibility checks.
- Delay: it uses `customDelay` exactly if given (no jitter or `backoffLimit`), else `#calculateDelay`.
- In `#retryFromError`, a `customRequest` replaces the request (via `#assignRequest` and `#withManagedSignal`) before `beforeRetry` hooks run, which see the `ForceRetryError`.

A normal retry starts from a failed fetch or an `HTTPError`, and goes through all those checks and the retry-timing headers.

## requests-get-flow

Trace a call to `requests.get(url)` down to the network: which functions and classes run, in order?

Points:
1. requests.get calls api.request, which opens a Session in a with block and calls session.request
2. Session.request builds a Request and calls prepare_request (merging session headers, cookies, auth and hooks into a PreparedRequest)
3. Session.request calls merge_environment_settings (proxies, verify, cert from the environment) and then Session.send
4. Session.send picks an adapter with get_adapter and calls HTTPAdapter.send, which uses a urllib3 connection pool (urlopen) and build_response
5. After the adapter, send dispatches the response hooks, extracts cookies into the session jar and follows redirects with resolve_redirects

### requests-get-flow/d350

Trace (bodies are compressed, so some bodies are elided; order inferred from visible code and docstrings):

1. `api.get(url, params=None, **kwargs)` calls `api.request("get", url, ...)`.
2. `api.request` uses `with sessions.Session() as session:` and calls `session.request(...)`.
3. `Session.request`: builds `Request(...)`, then `Session.prepare_request` (merges cookies, netrc auth, creates `PreparedRequest` and calls its `prepare`, which runs `prepare_method/url/headers/cookies/body/auth/hooks`). Then `merge_environment_settings` (proxies, verify, cert), then `Session.send`.
4. `Session.send`: `get_adapter(url)` → `HTTPAdapter`; `adapter.send(request, ...)`; then `dispatch_hook("response", ...)`, cookie extraction, and `resolve_redirects` if redirects are allowed.
5. `HTTPAdapter.send`: `get_connection_with_tls_context` (→ `build_connection_pool_key_attributes`, `poolmanager`/`proxy_manager_for`), `request_url`, then `conn.urlopen(...)` (urllib3), then `build_response` → `Response`.

Missing: urllib3's `urlopen` internals and the actual socket I/O aren't in the provided content. Also not shown: the `Session.__init__` body, which mounts the adapters.

### requests-get-flow/85ea

Call chain for `requests.get(url)`:

1. `requests.api.get(url, params=None, **kwargs)` calls `requests.api.request("get", url, ...)`.
2. `request()` opens `with sessions.Session() as session:` and calls `session.request(method, url, **kwargs)`.
3. `Session.request` builds a `Request`, calls `Session.prepare_request` (which uses `PreparedRequest.prepare` and merges cookies and auth), then `Session.merge_environment_settings` (proxies, verify, cert), then `Session.send`.
4. `Session.send` picks an adapter with `Session.get_adapter` (`HTTPAdapter`) and calls `adapter.send`. Afterwards it runs `dispatch_hook` and `resolve_redirects` (a `SessionRedirectMixin` method) and builds the response.
5. `HTTPAdapter.send` calls `get_connection_with_tls_context` (or `get_connection`), which returns a urllib3 pool from the `PoolManager`. It then calls `cert_verify`, `request_url`, and `add_headers`. Next it calls `conn.urlopen(...)` (urllib3 `HTTPConnectionPool.urlopen`).
6. urllib3 then runs `_make_request`, which calls `HTTPConnection.request` and `getresponse` over the socket.
7. `HTTPAdapter.build_response` wraps the result in a `Response`.

### requests-get-flow/901a

Trace (bodies are compressed, so some calls are inferred from docstrings/signatures):

1. `api.get(url, params=None, **kwargs)` → `api.request("get", url, ...)`. It uses `with sessions.Session() as session:` and calls `session.request(...)`.
2. `Session.__init__` sets up default headers and mounts `HTTPAdapter`.
3. `Session.request`: builds `Request`, calls `prepare_request` (`merge_cookies`, `PreparedRequest.prepare` → `prepare_url`, `prepare_headers`, `prepare_body`, `prepare_auth`, `prepare_cookies`, `prepare_hooks`), then `merge_environment_settings` and `Session.send`.
4. `Session.send`: `get_adapter(url)`, then `adapter.send(request, **kwargs)`, then `dispatch_hook("response", ...)`, then `resolve_redirects` if redirects are allowed.
5. `HTTPAdapter.send`: `get_connection_with_tls_context` (→ `build_connection_pool_key_attributes`, `select_proxy`, `poolmanager.connection_from_host`), `request_url`, then `conn.urlopen(...)` in urllib3, then `build_response`.

Missing: the bodies of the `HTTPAdapter` methods and `urllib3` internals (the socket connect and write) are not shown.

### requests-get-flow/4012

1. `api.get` → `api.request("get", ...)`, which opens `sessions.Session()` and calls `Session.request`.
2. `Session.request` builds a `Request`, then `prepare_request` → `PreparedRequest.prepare` (prepare_method/url/headers/cookies/body/auth/hooks).
3. `merge_environment_settings` resolves proxies, verify, and cert, then `Session.send`.
4. `Session.send` calls `get_adapter` (returns the mounted `HTTPAdapter`), then `HTTPAdapter.send`.
5. `HTTPAdapter.send` runs `get_connection_with_tls_context` (via `build_connection_pool_key_attributes`, `PoolManager.connection_from_host`), then `cert_verify`, `request_url`, `add_headers`.
6. It calls urllib3's `conn.urlopen(...)`, which is the network step. Its internals live in urllib3 and are not shown here.
7. `build_response` makes the `Response`.
8. Back in `Session.send`: `dispatch_hook`, cookie extraction, and `resolve_redirects` if redirects are allowed. Unless `stream` is set, `r.content` is read via `iter_content`.

Not shown: urllib3's socket, TLS, and HTTP handling.

### requests-get-flow/6e4f

1. `api.get` → `api.request("get", ...)`, which opens `sessions.Session()` and calls `Session.request`.
2. `Session.request` builds a `Request`, then `prepare_request` (merges cookies/headers, `get_netrc_auth`) → `PreparedRequest.prepare` (`prepare_method`, `prepare_url`, `prepare_headers`, `prepare_cookies`, `prepare_body`, `prepare_auth`, `prepare_hooks`).
3. `merge_environment_settings`, then `Session.send`.
4. `Session.send` calls `get_adapter` → `HTTPAdapter.send`.
5. `HTTPAdapter.send`: `get_connection_with_tls_context` (`select_proxy`, `build_connection_pool_key_attributes`, then `poolmanager.connection_from_host`), `cert_verify`, `request_url`, `add_headers`. It then calls `conn.urlopen(...)` on urllib3's connection pool.
6. `build_response` makes the `Response`. Back in `Session.send`: `dispatch_hook`, cookie extraction, `resolve_redirects` if redirects are allowed, and `r.content` unless streaming.

Missing: the actual socket, DNS and TLS work happens inside urllib3 (`urlopen`), which isn't in the provided content.

### requests-get-flow/46de

Call chain for `requests.get(url)`:

1. `requests.api.get(url, params=None, **kwargs)` calls `request("get", url, ...)`.
2. `requests.api.request` opens `with sessions.Session() as session:` and calls `session.request(method=..., url=...)`.
3. `Session.request` builds a `Request`, calls `self.prepare_request(req)` (which creates a `PreparedRequest` via `prepare()`, merging cookies, auth, and headers), then `self.merge_environment_settings(...)` and `self.send(prep, **send_kwargs)`.
4. `Session.send` calls `self.get_adapter(url)` to pick an `HTTPAdapter`, then `adapter.send(request, ...)`. Afterwards it runs `dispatch_hook`, extracts cookies, and `resolve_redirects` (a generator over redirects).
5. `HTTPAdapter.send` calls `self.get_connection_with_tls_context(...)` (older versions: `get_connection`), which returns a urllib3 `PoolManager` connection pool. It then calls `conn.urlopen(...)`.
6. urllib3 `HTTPConnectionPool.urlopen` calls `_make_request`, which opens the socket and sends the request via `HTTPConnection.request`/`getresponse`.
7. `HTTPAdapter.build_response` wraps the result in a `Response`.

I'm working from memory of this code, not from the files, so details in the current main branch (e.g. the TLS-context helper names) may differ slightly.

## requests-redirects

How does a requests Session follow redirects, and when does it drop the Authorization header on a redirect?

Points:
1. Session.send calls resolve_redirects, a generator, when allow_redirects is true
2. Each hop reads the Location header (get_redirect_target), consumes and closes the old response, and stops with TooManyRedirects at max_redirects (30)
3. rebuild_method changes the method (303 and 302 to GET unless HEAD, 301 only for POST)
4. rebuild_auth removes the Authorization header when should_strip_auth is true, which happens when the hostname changes
5. An http to https upgrade on the same host with default ports keeps the header; other scheme or port changes strip it

### requests-redirects/5aa5

**Following redirects:** `Session.send()` calls `adapter.send()`, then, if `allow_redirects` is true, iterates `SessionRedirectMixin.resolve_redirects()`. That generator uses `get_redirect_target()` (which relies on `Response.is_redirect`) and copies the request. It checks `max_redirects` (raising `TooManyRedirects`) and resolves relative URLs with `urljoin`/`requote_uri`. It then calls `rebuild_method()` (303/302/301 → GET), purges body headers, re-extracts cookies, and calls `rebuild_proxies()` and `rebuild_auth()`. Then it re-sends via `self.send(..., allow_redirects=False)`.

**Dropping Authorization:** `rebuild_auth()` deletes the `Authorization` header if `should_strip_auth(old_url, new_url)` returns True. That happens when:
- the hostname differs; or
- the scheme or port changed.

Exceptions that keep it: http→https on standard ports (80/None → 443/None), and the same scheme with default ports. `rebuild_auth()` then may re-apply auth via `get_netrc_auth()` if `trust_env` is set.

### requests-redirects/f151

`Session.send` sends the request, then, if `allow_redirects` is true, iterates over `self.resolve_redirects(r, request, ...)` and collects the responses into `r.history`.

`SessionRedirectMixin.resolve_redirects` is a generator. For each redirect response it:
- gets the target via `get_redirect_target` (reads the `Location` header, decodes it as UTF-8)
- builds the absolute URL (handles fragments, scheme-relative URLs, and relative URLs via `urljoin`)
- copies the prepared request and calls `rebuild_method` (e.g. 303 turns into GET, and 302 turns POST into GET)
- drops body headers when the method changes
- calls `rebuild_auth`
- optionally rebuilds proxies with `rebuild_proxies`
- sends the request with `self.send(..., allow_redirects=False)`
- raises `TooManyRedirects` past `max_redirects`

**Authorization dropping:** `rebuild_auth` deletes the `Authorization` header if `should_strip_auth(old_url, new_url)` is true. It then re-applies `.netrc` credentials if `trust_env` is on.

`should_strip_auth` returns False for the same host and an http→https upgrade on the default ports (80→443), or the same scheme and port. Otherwise, it returns True when the hostname differs, the port differs, or the scheme changes (other than that upgrade).

I'm working from memory of this code rather than having checked the current main branch, so details may have changed.

### requests-redirects/78bc

**Redirect flow:** `Session.send` calls `adapter.send`, then, if `allow_redirects`, iterates `resolve_redirects` (from `SessionRedirectMixin`). For each response, `get_redirect_target` reads the `Location` header. The loop copies the request (`req.copy()`), resolves the URL (relative, scheme-less, fragment handling), and calls `rebuild_method` (e.g. 303/302/301-POST become GET). It also purges body headers, copies cookies, calls `rebuild_proxies` and `rebuild_auth`, rewinds the body (`rewind_body`) if needed, and re-sends via `self.send`. History is collected in `Session.send`.

**Authorization dropping:** `rebuild_auth` deletes the `Authorization` header when `should_strip_auth(old_url, new_url)` returns true. That is when the hostname changes, or the scheme/port changes. The exception is an http→https upgrade on default ports (80→443). After stripping, `rebuild_auth` may reapply auth from `.netrc` via `get_netrc_auth` if `trust_env` is set.

Part not fully shown: the exact condition lines inside `should_strip_auth` are compressed away.

### requests-redirects/cd10

`Session.send` calls `Session.resolve_redirects` (a generator in `SessionRedirectMixin`) when `allow_redirects=True`. For each response, `get_redirect_target` reads the `Location` header (for 301/302/303/307/308 responses) and decodes it to a str. `resolve_redirects` then:

- Copies the previous request.
- Builds an absolute URL (handles scheme-relative and relative URLs, and strips fragments).
- Calls `rebuild_method` (e.g. POST→GET for 303, and 302/301 POST per browser behavior).
- Drops body headers (`Content-Length`, `Content-Type`, `Transfer-Encoding`) when the request becomes a GET without a body.
- Calls `rebuild_auth` and `rebuild_proxies`.
- Enforces `max_redirects`, raising `TooManyRedirects`.

**Authorization dropping:** `rebuild_auth` deletes the `Authorization` header if `should_strip_auth(old_url, new_url)` returns True. That function returns True when the hostname changes, except for the safe upgrade case of http→https on the same host with default ports (80→443, or no explicit port). It also returns True for a scheme or port change otherwise. If `trust_env` is on, it may then re-apply credentials from `.netrc` for the new URL.

### requests-redirects/a54e

**Following redirects:** `Session.send` calls `adapter.send`, then, if `allow_redirects` is true, iterates `resolve_redirects` (from `SessionRedirectMixin`). That generator uses `get_redirect_target` (relies on `Response.is_redirect`) to find the Location. It copies the request, consumes and closes the old response, enforces `max_redirects` (raises `TooManyRedirects`), resolves relative URLs, calls `rebuild_method` (303/302/301-POST become GET), and purges body headers unless 307/308. It also rebuilds cookies, calls `rebuild_proxies` and `rebuild_auth`, rewinds the body via `rewind_body`, then re-sends.

**Dropping Authorization:** `rebuild_auth` deletes `Authorization` if `should_strip_auth(old_url, new_url)` is true. That returns true when hostnames differ. It returns false for http→https on default ports (80/None → 443/None), and when the scheme is unchanged and both ports are default. Otherwise it returns true if the port or scheme changed. Afterward, if `trust_env` is set, `get_netrc_auth` may reapply netrc credentials via `prepare_auth`.

### requests-redirects/e656

**Redirect following:** `Session.send` calls the adapter, then (if `allow_redirects`) iterates `resolve_redirects`. For each hop, that generator:
- gets the target via `get_redirect_target` (the Location header);
- copies the request with `req.copy()`;
- normalizes and joins the URL with `urljoin` and `requote_uri`;
- calls `rebuild_method` to adjust the verb (303, 302 and 301-POST become GET);
- purges body headers;
- calls `rebuild_auth` and `rebuild_proxies`;
- rewinds the body with `rewind_body`;
- re-sends via `self.send`.

**Authorization dropping:** `rebuild_auth` compares the original and new URLs using `should_strip_auth`. If it returns true, the `Authorization` header is deleted. It then tries `get_netrc_auth` for the new URL when `trust_env` is set.

`should_strip_auth` compares parsed old and new URLs. The body of this function is mostly elided here. I can see the http→https default-port exception and the "root URI must match" comment. The exact comparison conditions (hostname, port and scheme) are not shown, so I can't state them precisely.

## requests-adapters

How does requests pick which transport adapter handles a URL, and how do you plug in your own adapter?

Points:
1. A new Session mounts an HTTPAdapter for both https:// and http://
2. get_adapter loops over the mounted adapters and returns the first whose prefix matches the start of the URL, case-insensitively
3. mount keeps the adapters ordered by prefix length, longest first, so the most specific prefix wins
4. If no prefix matches, get_adapter raises InvalidSchema
5. To plug in your own: subclass BaseAdapter (or HTTPAdapter), implement send and close, and call session.mount(prefix, adapter)

### requests-adapters/b8ee

`Session.get_adapter(url)` picks the adapter. It iterates over `self.adapters`, an `OrderedDict` mapping URL prefixes to adapter instances. It compares `url.lower().startswith(prefix.lower())` and returns the first match. If nothing matches, it raises `InvalidSchema` ("No connection adapters were found for ...").

`Session.__init__` sets up the defaults by calling `self.mount("https://", HTTPAdapter())` and `self.mount("http://", HTTPAdapter())`.

`Session.send()` calls `get_adapter(url=request.url)` and then `adapter.send(...)`.

**Plugging in your own adapter:**
1. Subclass `requests.adapters.BaseAdapter`, or `HTTPAdapter`. Implement `send(request, stream, timeout, verify, cert, proxies)` and `close()`.
2. Register it with `session.mount(prefix, adapter)`.

`mount()` inserts the adapter and then re-sorts the keys so that longer prefixes come first. This makes the most specific prefix win. For example, `https://api.example.com/` is matched before `https://`.

Example: `s.mount("https://api.example.com/", MyAdapter())`.

### requests-adapters/be07

**Selection:** `Session.send()` calls `Session.get_adapter(url=request.url)` and then `adapter.send(request, **kwargs)`. `get_adapter` is only partly visible: its docstring says it returns the appropriate adapter for the URL, and a comment says "Nothing matches". The matching loop and the error it raises are missing from the excerpt. Presumably it compares the URL to the mounted prefixes in `self.adapters`, but the code doesn't show this.

**Plugging in:** `Session.mount(prefix, adapter)` registers an adapter for a prefix. It keeps `self.adapters` ordered by descending prefix length (moving shorter keys after the new one), so longer, more specific prefixes win. An adapter should subclass `BaseAdapter`, which defines `send()` and `close()`, or `HTTPAdapter`, which you can customize via `init_poolmanager`, `proxy_manager_for`, and `add_headers`. The `HTTPAdapter` docstring shows `s.mount('http://', a)`.

**Missing:** The default mounts in `Session.__init__` aren't shown, so the exact default prefixes aren't visible.

### requests-adapters/8f51

**Selection:** `Session.send()` calls `Session.get_adapter(url)`. That method iterates `self.adapters` (an `OrderedDict`) and returns the first adapter whose prefix matches `url.lower().startswith(prefix.lower())`. If none match, it raises `InvalidSchema`. `Session.__init__` mounts `HTTPAdapter()` for `"https://"` and `"http://"`.

**Ordering:** `Session.mount(prefix, adapter)` stores the adapter, then moves every key shorter than the new prefix to the end. This keeps the dict in descending prefix-length order, so the longest prefix is checked first.

**Plugging in your own:**
1. Subclass `BaseAdapter` and implement `send()` and `close()`. Or subclass `HTTPAdapter` and override hooks such as `init_poolmanager`, `cert_verify`, `build_response`, or `add_headers`.
2. Call `session.mount("https://example.com/", MyAdapter())`.

`HTTPAdapter(max_retries=3)` can be mounted the same way, as the docstring shows.

### requests-adapters/ad25

**Selection:** `Session.send()` calls `Session.get_adapter(url=request.url)`, then `adapter.send(request, ...)`. `get_adapter` matches the URL against the keys of `self.adapters` (a `MutableMapping[str, BaseAdapter]`) by prefix. If nothing matches, it raises an error (the exact exception and the lowercase-comparison detail are cut off in the compressed content, so I can't confirm them).

**Ordering:** `Session.mount(prefix, adapter)` registers an adapter and keeps `adapters` sorted by descending prefix length. It does this by moving shorter keys (`keys_to_move`) after the new one. So the longest matching prefix is checked first.

**Defaults:** `Session.__init__` sets up the default adapters, but the body is elided here, so I can't show the exact prefixes (presumably `HTTPAdapter` for http/https).

**Plugging in your own:** Subclass `BaseAdapter` (implement `send()` and `close()`) or subclass `HTTPAdapter`, then call `session.mount("prefix://", adapter)`. For example: `s.mount('http://', HTTPAdapter(max_retries=3))`.

### requests-adapters/90a2

**Selection:** `Session.send()` calls `Session.get_adapter(url)`, which iterates `self.adapters` (an `OrderedDict`) and returns the first adapter whose prefix matches `url.lower().startswith(prefix.lower())`. If none match, it raises `InvalidSchema`. `Session.__init__` mounts `HTTPAdapter()` for `"https://"` and `"http://"`.

**Ordering:** `Session.mount(prefix, adapter)` stores the adapter, then moves every key shorter than the new prefix to the end. This keeps longer prefixes first, so the most specific match wins.

**Plugging in your own:**
1. Subclass `BaseAdapter` (implement `send()` and `close()`) or subclass `HTTPAdapter` and override pieces such as `init_poolmanager`, `cert_verify`, `add_headers`, or `get_connection_with_tls_context`.
2. Call `session.mount("prefix://", MyAdapter())`.

`Session.send()` then calls `adapter.send(request, **kwargs)`, and `Session.close()` calls each adapter's `close()`.

### requests-adapters/32e3

`Session.get_adapter(url)` picks the adapter. It iterates over `self.adapters`, an `OrderedDict` mapping URL prefixes to adapter instances. It lowercases the URL and returns the first adapter whose prefix the URL starts with (`url.lower().startswith(prefix.lower())`). If nothing matches, it raises `InvalidSchema`.

`Session.__init__` sets up the defaults by calling `self.mount("https://", HTTPAdapter())` and `self.mount("http://", HTTPAdapter())`.

`Session.send` calls `get_adapter(url=request.url)` and then `adapter.send(request, **kwargs)`.

**Plugging in your own:**
1. Subclass `requests.adapters.BaseAdapter`, or `HTTPAdapter`. Implement `send(request, stream, timeout, verify, cert, proxies)` and `close()`.
2. Register it with `session.mount(prefix, adapter)`.

`Session.mount` stores the adapter under the prefix, then re-sorts `self.adapters` so that longer prefixes come first. This makes the most specific prefix win. For example, `https://api.example.com/` takes precedence over `https://`.

```python
s = requests.Session()
s.mount("https://api.example.com/", MyAdapter())
```

## requests-env-settings

Where do proxy and CA bundle settings from environment variables get merged into a requests call, and which source wins when they conflict?

Points:
1. Session.request calls merge_environment_settings after preparing the request, and it only reads the environment when trust_env is true
2. Environment proxies come from get_environ_proxies (respecting no_proxy) and are added with setdefault, so proxies passed explicitly win
3. REQUESTS_CA_BUNDLE, then CURL_CA_BUNDLE, replace verify when verify is True or None
4. Then merge_setting combines request-level and session-level values, with the request-level value winning
5. Consequence: session.verify = False can be overridden by a CA bundle from the environment, because the request-level verify is None and gets the env value first

### requests-env-settings/60d2

**Where merging happens:** `Session.request` calls `Session.merge_environment_settings(prep.url, proxies, stream, verify, cert)`. Later, `Session.send` uses `resolve_proxies` if no `proxies` kwarg is present, and redirects use `rebuild_proxies`.

**Proxies:** If `trust_env` is true, `get_environ_proxies` (which uses `should_bypass_proxies` and `getproxies`) supplies the environment proxies. These are added with `proxies.setdefault(k, v)`, so request-level proxies win over the environment. Then `merge_setting(proxies, self.proxies)` combines the result with session proxies, and the request value wins over the session value.

**CA bundle:** If `verify` is `True` or `None`, `REQUESTS_CA_BUNDLE` is used, then `CURL_CA_BUNDLE`, then the original `verify`. An explicit `verify=False` or a path string is not overridden by the environment. `merge_setting(verify, self.verify)` follows.

**Missing:** The repository does not show the exact priority between a session-level `verify` and the environment, since `merge_setting` is called on the already-overridden value. It also does not show the `getproxies` internals, which come from the standard library.

### requests-env-settings/fc60

It happens in `Session.merge_environment_settings` (requests/sessions.py), called from `Session.request` after the `Request` is prepared, just before `send`.

**Proxies:** If `trust_env` is true, it calls `get_environment_proxies` (utils, uses `getproxies` plus `no_proxy` handling via `should_bypass_proxies`) and uses `setdefault` to fill in the request's `proxies` for any scheme not already set. Then `merge_setting(proxies, self.proxies)` combines per-request and session proxies.

**CA bundle/verify:** If `trust_env` is true and `verify` is `True` or `None`, it takes `REQUESTS_CA_BUNDLE`, falling back to `CURL_CA_BUNDLE`. Then `merge_setting(verify, self.verify)` is applied.

**Precedence:** Explicit per-request args > session attributes (`session.proxies`, `session.verify`) > environment variables. A caveat: for proxies, env values are merged into the request-level dict, so they can override session-level proxies. An explicit `verify=False` or a path suppresses the env CA bundle.

### requests-env-settings/cb61

**Where:** `Session.request` (sessions.py) calls `Session.merge_environment_settings`. When `trust_env` is on, that method:
- calls `get_environ_proxies(url, no_proxy=...)` for proxies (which uses `should_bypass_proxies`/`get_proxy` in utils.py);
- has a comment about reading requests environment configuration "compatible with cURL" for `verify` (the CA bundle);
- then merges everything via `merge_setting` for `proxies`, `stream`, `verify` and `cert`.

On redirects, `rebuild_proxies` calls `resolve_proxies` (utils.py) to redo the proxy resolution.

**Who wins:**
- Between request-level and session-level settings, `merge_setting` is visible: request values override session values, and keys set to `None` are removed.
- Between environment and explicit settings, the compressed file drops the lines that apply the environment values, so I can't confirm the precedence.
- The environment variable names (e.g. for the CA bundle) are also not visible.

### requests-env-settings/01d4

It happens in `Session.merge_environment_settings` (requests/sessions.py), called from `Session.request` after the `Request` is prepared.

- **Proxies:** if `trust_env` is true, it calls `get_environment_proxies` (utils; this was `get_environ_proxies` in older versions) with the URL and `no_proxy`. It then runs `setdefault` on the result for each entry in the request's `proxies`. After that, `merge_setting(proxies, self.proxies)` combines the request and session proxies. Precedence is request `proxies` > session `proxies` > environment variables. Environment proxies are also skipped for hosts matching `no_proxy`. `resolve_proxies` in `Session.rebuild_proxies` re-applies this on redirects.

- **CA bundle:** if `trust_env` is true and both `verify` is `True`/None and the request/session didn't set one, it takes `REQUESTS_CA_BUNDLE`, or else `CURL_CA_BUNDLE`. Then `merge_setting` merges `verify` with `self.verify`. Precedence is explicit request `verify` > env vars > session `verify`.

I'm working from memory, not the current main source, so check the details against `sessions.py`.

### requests-env-settings/0154

**Where it happens:** `Session.request` calls `Session.merge_environment_settings`. That method:
- reads `trust_env`;
- takes `no_proxy` from the passed `proxies`;
- calls `get_environ_proxies` (which uses `should_bypass_proxies`) for proxy variables;
- checks the environment for a CA bundle ("compatible with cURL"), assigning the result to `verify`;
- finishes with `merge_setting` for `proxies`, `stream`, `verify` and `cert`.

On redirects, `rebuild_proxies` re-evaluates the proxy settings through `resolve_proxies`.

**Precedence:**
- `merge_setting` is visible. A per-request value overrides the session value, and `None` keys are dropped. A non-dict setting such as `verify` is handled by an early bypass whose body isn't shown.
- The environment is only consulted when `trust_env` is true.

**Missing:** The compressed content strips the lines that apply the environment values. That includes the exact CA bundle variable names and whether environment proxies override or only fill in explicit `proxies` (`setdefault`-style). So I can't confirm which source wins in a conflict between environment and explicit settings.

### requests-env-settings/e4e5

**Where merged:** `Session.request` calls `Session.merge_environment_settings` (in `sessions.py`). Separately, `Session.send` calls `resolve_proxies` (utils.py) when `proxies` isn't in kwargs, and `rebuild_proxies` re-runs it on redirects.

**Proxies:** If `trust_env` is true, `merge_environment_settings` calls `get_environ_proxies` (which uses `should_bypass_proxies` and `getproxies`). It uses `proxies.setdefault`, so explicit per-request proxies beat environment ones. Then `merge_setting(proxies, self.proxies)` combines the request and session settings. Request values override session values, and `None` values are removed.

**CA bundle:** If `verify` is `True` or `None`, the code uses `REQUESTS_CA_BUNDLE`, then `CURL_CA_BUNDLE`, then falls back to the original `verify`. An explicit `verify` string or `False` on the call skips the environment. `merge_setting(verify, self.verify)` follows. Because the environment value replaces `verify=None`, it wins over `Session.verify`.

**Not shown:** `getproxies` internals (stdlib), and how `cert_verify` handles the final value beyond `adapters.py`.

## requests-hooks-cookies

In requests, what happens to cookies set by a response, and how do response hooks run?

Points:
1. Session.send calls dispatch_hook('response', hooks, r) right after the adapter returns
2. dispatch_hook calls each hook in turn, and a hook that returns a value replaces the response passed on
3. Session hooks and request hooks are merged in prepare_request (merge_hooks)
4. After the hooks, extract_cookies_to_jar stores the response cookies in the session's cookie jar (also for responses in history)
5. During redirects, cookies are extracted again and the next request's Cookie header is rebuilt from the jar

### requests-hooks-cookies/d255

**Cookies:** In `HTTPAdapter.build_response`, cookies from the server are added to `response.cookies` (via `extract_cookies_to_jar`, which wraps the headers in `MockRequest`/`MockResponse` for `jar.extract_cookies`). Then in `Session.send`, after hooks run, the session persists them into `self.cookies` with `extract_cookies_to_jar` (the code is compressed, but the comments say "Persist cookies"). If hooks create history, those responses' cookies are also extracted. During redirects, `resolve_redirects` extracts the response cookies into the new request's `_cookies` jar. It then calls `prepare_cookies` to rebuild the Cookie header (`get_cookie_header`).

**Hooks:** `Session.send` calls `dispatch_hook("response", hooks, r, **kwargs)` right after `adapter.send`. `dispatch_hook` normalizes the hooks into a list and calls each hook with the data. If a hook returns non-None, that value replaces the response.

Missing: exact `extract_cookies_to_jar` call lines in `send` are stripped from the compressed content.

### requests-hooks-cookies/3688

**Cookies:** In `HTTPAdapter.build_response`, `extract_cookies_to_jar` puts the server's cookies into `response.cookies`. In `Session.send`, after hooks run, `extract_cookies_to_jar` is called again for `self.cookies`, first for any `r.history` responses and then for the final response. `extract_cookies_to_jar` wraps the request and response in `MockRequest` and `MockResponse` so `CookieJar.extract_cookies` can read them. During redirects, `resolve_redirects` extracts cookies into the new request's jar, merges them with `self.cookies` via `merge_cookies`, and calls `prepare_cookies`. Outgoing `Cookie` headers come from `get_cookie_header`.

**Hooks:** `Session.prepare_request` merges hooks with `merge_hooks`, and `PreparedRequest.prepare_hooks` registers them via `register_hook`. After `adapter.send`, `Session.send` calls `dispatch_hook("response", hooks, r, **kwargs)`. That function calls each hook with the response and replaces the response if a hook returns non-None. `HTTPDigestAuth` registers `handle_401` and `handle_redirect` this way.

### requests-hooks-cookies/b4a0

**Cookies:** In `HTTPAdapter.build_response`, the adapter adds the server's cookies to `response.cookies` (via `extract_cookies_to_jar`, in `cookies.py`, which wraps the request/response with `MockRequest`/`MockResponse` for `CookieJar.extract_cookies`). In `Session.send`, after hooks run, the session persists cookies into `self.cookies` (the compressed source hides the exact call, but a comment says "Persist cookies"). It does so again for any hook-created history. During redirects, `resolve_redirects` extracts response cookies into the new request's jar. For outgoing requests, `prepare_cookies` uses `get_cookie_header`.

**Hooks:** `Session.send` calls `dispatch_hook("response", hooks, r, **kwargs)` right after `adapter.send`. `dispatch_hook` (hooks.py) wraps a single hook in a list, calls each hook with the response, and replaces the data only if a hook returns non-None. Hooks are registered via `register_hook` and `prepare_hooks`.

**Missing:** the exact lines of the "Persist cookies" calls were stripped by compression.

### requests-hooks-cookies/855d

**Cookies:** In `Session.send` (after `adapter.send`), `extract_cookies_to_jar(self.cookies, request, r.raw)` stores the response's `Set-Cookie` headers in the session jar. The adapter's `HTTPAdapter.build_response` also calls `extract_cookies_to_jar(response.cookies, req, resp)` to populate `Response.cookies`. During redirects, `Session.resolve_redirects` calls `extract_cookies_to_jar(prepared_request._cookies, ...)` and `rebuild_auth`/`prepare_cookies` to carry cookies forward. On later requests, `Session.prepare_request` merges them via `merge_cookies` and `get_cookie_header`.

**Hooks:** `Session.send` calls `dispatch_hook("response", hooks, r, **kwargs)` after the adapter returns. `dispatch_hook` (in `hooks.py`) iterates over the callables for that event (wrapping a single callable into a list), calls each with `hook(hook_data, **kwargs)`, and if a hook returns non-None, that value replaces `hook_data`. Hooks are registered via `RequestEncodingMixin`/`Request.register_hook`, and `Request.prepare` → `PreparedRequest.prepare_hooks` attaches them. Hooks are merged from request and session via `merge_hooks`.

### requests-hooks-cookies/09eb

**Cookies:** In `HTTPAdapter.build_response`, `extract_cookies_to_jar(response.cookies, req, resp)` fills the response's jar. `extract_cookies_to_jar` wraps the request in `MockRequest` and the headers in `MockResponse`, then calls `jar.extract_cookies`. In `Session.send`, after hooks run, it calls `extract_cookies_to_jar(self.cookies, request, r.raw)`, and does the same for each `r.history` entry. During `resolve_redirects`, cookies are extracted into the new request's jar and merged with `merge_cookies`. Then `prepare_cookies` builds the `Cookie` header via `get_cookie_header`. After each redirected response, they are extracted again into `self.cookies`.

**Hooks:** `Request.__init__` and `PreparedRequest.prepare_hooks` register them via `register_hook`. `Session.prepare_request` merges request and session hooks with `merge_hooks`. `Session.send` calls `dispatch_hook("response", hooks, r, **kwargs)`. This calls each hook and replaces the response if a hook returns non-None.

### requests-hooks-cookies/3319

**Cookies:** In `Session.send` (sessions.py), after the adapter returns the response, it calls `extract_cookies_to_jar(self.cookies, request, r.raw)` (cookies.py). That wraps the request/response in `MockRequest`/`MockResponse` and calls `jar.extract_cookies`, storing the cookies in the session's `RequestsCookieJar`. `resolve_redirects` does the same for each redirect hop, and `Session.prepare_request` merges the jar via `merge_cookies` and `get_cookie_header` (applied in `prepare_cookies`) for later requests. `HTTPAdapter.build_response` also extracts cookies into `response.cookies`.

**Hooks:** In `Session.send`, after the response is received, `dispatch_hook("response", hooks, r, **kwargs)` (hooks.py) runs. `dispatch_hook` normalizes the hook(s) (a single callable or a list) and calls each in order with `hook(hook_data, **kwargs)`. If a hook returns non-`None`, that value replaces `hook_data` (the response). Hooks are registered via `register_hook` on `Request`/`PreparedRequest` (`prepare_hooks`) and merged by `merge_hooks`.

## diff-tour-flow

Trace what happens from running `tour feature/x` until the tour appears on screen: which functions run, in order?

Points:
1. main.run parses flags, loads the config and resolve() turns a branch name into base = the default branch and head = the branch
2. ui.NewGenerating / App.start computes a CacheKey and opens the saved tour with LoadCached if there is one
3. Otherwise generate.Generate runs in a goroutine: PrepareWorkspace exports the merge base and head with git archive into base/ and head/ plus change.diff and commits.txt, and writes the lessons
4. buildPrompt assembles the prompt and backend.Run runs the LLM CLI (for Claude Code, claude -p with stream-json) in that workspace
5. The reply is parsed and validated (settle, with up to 2 repair rounds), saved with SaveCached, and openViewer shows it

### diff-tour-flow/1103

Flow for `tour feature/x`:

1. `main` → `run(os.Args[1:])`.
2. `parse` collects flags and positionals (`["feature/x"]`); `config.Load` reads settings.
3. `i18n.Detect`/`Parse`/`Set` choose the language; `git.Open` opens the repo.
4. It's not `.json`, so the tour-file branch is skipped. `repo.RevParse("HEAD")` and `isTerminal` are checked. If `cfg == nil`, `runSetup` runs first.
5. `backendFor(cfg.LLM)` picks the backend.
6. `resolve(repo, "feature/x")` → `RevParse` on `refs/heads/` or `refs/remotes/`, then `repo.DefaultBranch()` as base, returning base and head.
7. `ui.NewGenerating` computes `generate.CacheKey` and sets the generating screen.
8. `tea.NewProgram(app).Run()` → `App.Init` → `App.start`.
9. `start` calls `generate.LoadCached`. On a cache hit, `openViewer` creates the `Viewer` via `NewViewer`, and `Viewer.View` calls `render.Frame`. On a miss, `generate.Generate` runs in a goroutine, then `SaveCached`, then `doneMsg` → `openViewer`.

Not shown: the prompt files and the internals of the bubbletea runtime.

### diff-tour-flow/1b58

Only the signatures of `main.go` are visible; its function bodies were stripped by compression. So the exact order inside `main`/`run` is partly missing.

What the content shows:

1. `main()` → `run(args)`, with `parse(fs, args)` splitting flags from positionals.
2. `run` calls `resolve(repo, "feature/x")`. Per its comment, a branch name becomes (default branch, that branch), using `git.Repo` methods (`DefaultBranch`, `Resolve`, etc.).
3. `backendFor(llm)` picks the LLM backend. `runSetup` (via `ui.Setup`) runs on first use.
4. `ui.NewGenerating(repo, base, head, lang, backend)` builds the `App`; tea runs it (`Init`, then `start`).
5. `start` checks `generate.LoadCached` (key from `generate.CacheKey`). If cached, `openViewer` creates `ui.NewViewer`.
6. Otherwise it generates in a goroutine: `generate.Generate` → `PrepareWorkspace` → `buildPrompt` → `backend.Run` → `settle`/`parse` → `SaveCached`, then `openViewer`.
7. `Viewer.View` → `render.Frame`.

Missing: the bodies of `main`, `run`, `resolve` and `start`, so exact call order and branching are unconfirmed.

### diff-tour-flow/152a

NOT SURE

### diff-tour-flow/d7cc

NOT SURE

### diff-tour-flow/fe0e

Trace for `tour feature/x`:

1. `main` → `run(os.Args[1:])`.
2. `parse` reads flags and collects the positional arg `feature/x`.
3. `config.Load`; if nil, `runSetup` runs later. `i18n.Detect`/`Parse`/`Set` choose the language.
4. `filepath.Abs`, then `git.Open`. The arg isn't `.json`, so the tour-file path is skipped.
5. `repo.RevParse("HEAD")` and `isTerminal` checks. If cfg is nil, `runSetup` → `ui.NewSetup`, then `config.Load` again.
6. `backendFor(cfg.LLM)` picks the backend.
7. `resolve(repo, "feature/x")`: `RevParse` on `refs/heads/` or `refs/remotes/`, then `repo.DefaultBranch()` gives base.
8. `ui.NewGenerating` computes `generate.CacheKey`.
9. `tea.NewProgram(app).Run()` → `App.Init` → `App.start`.
10. `start` calls `generate.LoadCached`. On a hit, `openViewer` → `NewViewer`.
11. On a miss, it spawns a goroutine: `generate.Generate` (`PrepareWorkspace`, `buildPrompt`, `backend.Run`, `settle`/`parse`), then `SaveCached`, then `doneMsg` → `openViewer`.
12. `Viewer.View` → `render.Frame` draws the screen.

### diff-tour-flow/ce33

Partly traceable. The body of `run` in `cmd/tour/main.go` is compressed away, so its exact order isn't shown.

Visible order:

1. `main()` → `run(args)`, with `parse` splitting flags from positional args.
2. `run` calls `resolve(repo, "feature/x")`. Per its comments, it treats a branch name as "default branch up to that branch", and it also accepts remote-tracking branches. It uses `git.Repo` methods (`RevParse`, `Resolve`, `DefaultBranch`), but the body isn't shown.
3. `backendFor(llm)` picks the LLM backend from config (`config.Load`, possibly `runSetup`).
4. `ui.NewGenerating(...)` builds the `App`, probably run through `tea` (the call isn't visible).
5. `App.Init` → `App.start`: it uses a saved tour if `generate.LoadCached(CacheKey)` finds one. Otherwise a goroutine calls `generate.Generate` (`PrepareWorkspace`, `buildPrompt`, `Backend.Run`, `parse`/`settle`, `SaveCached`), sending `progressMsg`/`doneMsg`.
6. `App.openViewer` creates the `Viewer`. `Viewer.View` calls `render.Frame` → `render.Block` to draw the screen.

Missing: the body of `run`, including how it handles the cached tour check before generation.

## diff-tour-invalid

What does diff-tour do when the LLM's reply is not a valid tour?

Points:
1. extractJSON cuts the reply at a separator line and takes the text from the first { to the last }
2. If the reply has no JSON at all, the generation is run once more instead of asking for a fix
3. parse decodes with unknown fields disallowed, runs Validate, and render.Check verifies that referenced files, line ranges and hunks exist
4. The problems are sent back in a repair prompt, with the previous JSON written to draft.txt in the workspace
5. At most MaxRepairs = 2 repair rounds, then it fails with InvalidTourError listing the problems

### diff-tour-invalid/b12e

NOT SURE

### diff-tour-invalid/dd2a

`Generate` runs the backend, then hands the reply to `settle`. If `extractJSON` finds no JSON at all, `Generate` first reruns the backend once.

`settle` calls `parse`, which does the following:
- Extracts the JSON and decodes it with unknown fields disallowed.
- Pins base and head to the requested values.
- Runs `Tour.Validate`.
- Runs `render.Check`, which verifies that the referenced files, lines, hunks, schemas and files-map entries exist.

If problems are found, `settle` writes the draft to `draft.txt` in the workspace. It then calls `backend.Run` with `repairPrompt(problems)`, which lists the problems, and parses the new reply. It does this up to `MaxRepairs` (2) times and increments `stats.Repairs` each time, reporting "repairing" via `progress`.

If problems remain after that, `settle` returns `*InvalidTourError`. In `App.Update`, the `doneMsg` error calls `App.fail`, which shows the failed screen (`screenFailed`) with the message.

`Answer` reuses `settle` the same way.

Not shown: the contents of the prompt files (`fix.md` etc.).

### diff-tour-invalid/7cf7

`Generate` first runs the backend. If `extractJSON` finds no JSON in the reply, it reruns the backend once (a full regeneration, not a fix request).

Then `settle` calls `parse`, which does the following:
- `extractJSON` pulls the JSON out.
- A decoder with `DisallowUnknownFields` decodes it.
- `Tour.Validate` checks the format.
- `render.Check` verifies that the files, lines, hunks and schemas it references exist.

If `parse` returns problems, `settle` writes the draft to `draft.txt`, builds `repairPrompt` from the problems, and calls `backend.Run` again. It does this up to `MaxRepairs` (2) times, and `stats.Repairs` and the "repairing" progress are updated.

If problems remain, it returns `*InvalidTourError` listing them. In `App`, `Update` handles the `doneMsg` error by calling `fail`, which shows the "gen.failed" screen with the message. The same flow is used by `Answer`.

Not shown: the contents of the prompt files (`fix.md` and the others).

### diff-tour-invalid/b244

When the reply isn't a valid tour, `Generate` tries to repair it, and if that fails it returns an error.

- `parse` uses `extractJSON` to strip research notes, fences and preamble, then builds a `tour.Tour` and checks its format and the referenced files and lines. It returns a list of problems.
- `settle` handles those problems. While validation finds any, it calls the backend again with `repairPrompt(problems)`, up to `MaxRepairs` (2) times per reply. The pre-fix JSON is placed in `draft.txt` in the working directory. Each attempt is reported as a "repairing" `Progress`.
- If the reply has no JSON at all, `Generate` regenerates once instead of requesting a fix.
- If problems remain after the repairs, an `InvalidTourError{Problems}` is returned. In the UI, `App.fail` then shows the failure screen.

The function bodies are compressed out, so I can't confirm what happens if the single regeneration also yields no JSON, or the exact error text the UI shows.

### diff-tour-invalid/b4a9

NOT SURE

### diff-tour-invalid/7fce

Based on the signatures and comments shown (function bodies are compressed away, so exact logic is partly missing):

- `extractJSON` pulls JSON out of the reply, stripping research notes (split on `separator`), ```json fences and preamble.
- `parse` turns the reply into a `tour.Tour`, checks the format and that referenced files and lines exist, and returns problems as a list. It pins base and head to the requested values.
- `Generate` drives the process. Per its comment, if the reply has no JSON at all (e.g. it stopped midway), it regenerates once instead of asking for a fix.
- `settle` parses a reply and, while validation finds problems, calls `repairPrompt` to ask the LLM to fix only those errors, up to `MaxRepairs` (2) times per reply.
- If errors remain, an `InvalidTourError` carrying `Problems` is returned.

Not shown: the bodies of `Generate`/`settle`, so I can't confirm exactly how the retry and error are wired together or how the UI displays the failure (`App.fail` and `screenFailed` exist).

## diff-tour-sandbox

How does diff-tour keep the LLM from changing the user's repository or reading files outside it?

Points:
1. The LLM works in an exported copy (a workspace in the cache directory made with git archive), not in the repository
2. Extraction skips absolute paths and paths that climb out with .., and does not export symlinks
3. For Claude Code it passes --restricted and --tools Read,Grep,Glob (read-only tools only)
4. It also passes --permission-mode dontAsk and --strict-mcp-config (no MCP servers, no prompts)
5. The workspace is deleted when generation finishes

### diff-tour-sandbox/4c07

**Isolation by copy:** `PrepareWorkspace` exports base/head via `extract`/`extractArchive` (git archive, or `copyWorktree`) into a temp dir. `extractArchive` skips paths escaping the destination and exports no symlinks. The git package only runs read-only commands (`GIT_OPTIONAL_LOCKS=0`), so the LLM never touches the repo.

**Per-backend restrictions** (each runs with the workspace as cwd):
- `claudeArgs`: `--restricted`, `Read,Grep,Glob` tools only, `dontAsk`.
- `codexArgs`/`codexReadablePaths`: read-only sandbox profile, no network, tools disabled.
- `geminiPolicy`/`geminiOutsideRules`/`geminiSettings`/`geminiEnv`: admin policy allowing only read tools and denying paths outside the workspace.
- `openCodeConfig`/`openCodeEnv`: deny-all permissions, allow read/glob/grep.
- `copilotArgs`/`prepareCopilotHome`/`copilotEnv`: limited tools, shell and write denied, sanitized home.

**Gaps:** the code states Codex was verified only on Linux, and Gemini transcripts still go to `~/.gemini`. Actual enforcement inside each external CLI isn't visible here.

### diff-tour-sandbox/8dc1

**Isolation from the repo:** the LLM never sees the repository. `PrepareWorkspace` exports copies into a separate directory (`base/`, `head/`, diffs) via `extract`/`extractArchive` (git archive piped as tar) and `copyWorktree`. `extractArchive` rejects paths escaping the destination and skips symlinks. `git.Repo.run` uses only read-only commands and avoids incidental writes such as lock files.

**Read-only backends** (each `Run` is confined to that workspace):
- `claudeArgs`: restricted mode, only Read/Grep/Glob, no MCP, `dontAsk`.
- `codexArgs`/`codexReadablePaths`: a sandbox profile that can read only the workspace and system paths, with nothing writable and no network.
- `geminiArgs`, `geminiSettings`, `geminiPolicy`/`geminiOutsideRules`: allow only read tools and deny paths leaving the directory.
- `copilotArgs`/`copilotEnv`/`prepareCopilotHome`: limited tools, a sanitized home.
- `openCodeArgs`/`openCodeConfig`/`openCodeEnv`: only read/glob/grep, `external_directory` denial.

**Not shown:** the contents of the prompts and lessons files, and the actual enforcement inside each CLI.

### diff-tour-sandbox/f0b6

**Isolation by copy:** `PrepareWorkspace` exports base/head via `extract`, `extractArchive` (skips symlinks, rejects `..`/absolute paths) or `copyWorktree`. The LLM runs in that temp directory, never the repo. `git.Repo.run` uses only read commands with `GIT_OPTIONAL_LOCKS=0`.

**Per-backend restrictions:**
- `claudeArgs`: `--restricted`, tools Read/Grep/Glob, `dontAsk`.
- `codexArgs`: read-only sandbox profile, no network, `--ignore-user-config`, disabled tools.
- `geminiPolicy`/`geminiOutsideRules`/`geminiSettings`/`geminiEnv`: allow only read tools, deny paths outside the workspace.
- `openCodeConfig`/`openCodeEnv`: deny everything except read/glob/grep.
- `copilotArgs`/`prepareCopilotHome`/`copilotEnv`: only view/grep/glob, shell and write denied, sanitized home.

**Not shown:** the enforcement itself happens inside the external CLIs, so their actual behavior can't be verified from this code. Codex's comments say only Linux was verified.

### diff-tour-sandbox/0609

NOT SURE

### diff-tour-sandbox/c298

NOT SURE

### diff-tour-sandbox/931f

The LLM never touches the repository. It reads an exported copy:

- **Workspace:** `PrepareWorkspace` uses `extract` and `extractArchive` (from `git archive` via `Repo.Archive`) to write `base/` and `head/` into a temp dir. `extractArchive` rejects paths escaping the destination and skips symlinks. `copyWorktree` exports only tracked or non-ignored files. `Repo.run` uses read-only git commands.
- **Backend restrictions:** each CLI runs in that dir with read-only tools:
  - `claudeArgs`: Read/Grep/Glob only, `--restricted`, `dontAsk`.
  - `codexArgs`: a sandbox profile that can read only the workspace, plus `codexReadablePaths`.
  - `geminiArgs`, `geminiPolicy`, `geminiOutsideRules` and `geminiSettings`: an allowlist of tools, with deny rules for outside paths.
  - `copilotArgs`, `copilotEnv`, `prepareCopilotHome`: limited tools and a sanitized home.
  - `openCodeArgs`, `openCodeConfig`, `openCodeEnv`: `openCodeTools` only, plus the `external_directory` check.

The repository doesn't show how the CLIs enforce these flags internally.

## diff-tour-cache

When does diff-tour reuse a saved tour instead of asking the LLM again, and what makes it regenerate?

Points:
1. CacheKey is a sha256 over the merge base, the head commit, the language, PromptVersion and the backend name (first 24 hex chars)
2. Saved tours live in the user cache directory under tour/tours/<key>.json
3. App.start loads the saved tour with LoadCached and opens it without calling the LLM
4. A new commit on the branch, a different language or LLM, or a bumped PromptVersion gives a new key and a new tour
5. The branch picker shows which targets are already cached, and answers to reader questions are saved back into the cached tour

### diff-tour-cache/0359

**Reuse:** `App.start` (internal/ui/app.go) opens the saved tour if there is one and only otherwise starts generating. `FindTargets` marks each target `Cached` using its `Key`, and `LoadCached(key)` returns nil when nothing is stored. The key comes from `CacheKey(repo, base, head, lang, backend)`. Per its comment, it is the same for the same change, language, prompts and LLM.

**Regeneration:** a different base/head, language or backend gives a different key, so the tour is regenerated. Editing the prompts requires bumping `PromptVersion` (currently "v6") so saved tours are regenerated. For uncommitted changes, `worktreeID` (git/worktree.go) derives an identifier from the working-tree contents, so edits should change the key.

**Missing:** the bodies of `CacheKey`, `LoadCached`, `start` and `worktreeID` are compressed away. I can't confirm exactly which inputs are hashed, how `PromptVersion` enters the key, or where `SaveCached` is called. No expiry or force-regenerate option appears in the shown content.

### diff-tour-cache/526c

A saved tour is reused when `App.start` finds one: `FindTargets` marks targets `Cached` and gives each a `Key`, and `start` then opens the saved tour (via `LoadCached(key)`, with `openViewer`) instead of generating. Otherwise it generates in a goroutine via `generate.Generate`.

The key comes from `CacheKey(repo, base, head, lang, backend)`. It is the same for the same change, language, prompts, and LLM. So a new key, and therefore regeneration, results from:
- a different base or head (for the working tree, `worktreeID` derives from the file contents, so edits change it),
- a different language,
- a different LLM backend,
- a bumped `PromptVersion` (currently "v6"), which exists so saved tours get regenerated.

`CachePath` and `SaveCached` handle storage.

Not shown: the bodies of `CacheKey`, `LoadCached`, and `start`, so exactly which inputs are hashed, and how a corrupt or missing cache file is handled, aren't visible. Only the comments and signatures are.

### diff-tour-cache/3e33

NOT SURE

### diff-tour-cache/3017

NOT SURE

### diff-tour-cache/1134

**Reuse:** `App.start` (internal/ui/app.go) calls `generate.LoadCached(t.Key)`. If a saved tour exists at `CachePath(key)` (under the user cache dir, `tour/tours/<key>.json`), it opens it via `openViewer` without calling the LLM. `FindTargets` also calls `LoadCached` to mark targets "ready".

**Key:** `generate.CacheKey` hashes the merge base of base/head, `RevParse(head)`, language, `PromptVersion` ("v6"), and backend name.

**Regeneration:** If any of those change, the key changes and no cached file matches, so `generate.Generate` runs. Examples are new commits on head, a different merge base, another language or LLM, or a bumped `PromptVersion`. For the working tree, `RevParse` returns `worktreeID`, which hashes HEAD plus the diff, so any uncommitted edit regenerates.

**Within a run:** `settle` re-asks the LLM up to `MaxRepairs` times if validation fails. `Generate` reruns once if the reply has no JSON.

**Not shown:** no explicit cache-invalidation or delete command appears in the code above.

### diff-tour-cache/a473

**Reuse:** `App.start` (internal/ui/app.go) calls `generate.LoadCached(t.Key)`. If a tour file exists at `CachePath(key)` (`<UserCacheDir>/tour/tours/<key>.json`), it opens the viewer directly via `openViewer`, with no LLM call. `FindTargets` also uses `LoadCached` to mark branches as "ready".

**Regeneration:** The key comes from `generate.CacheKey`, a hash of:
- `repo.MergeBase(base, head)`
- `repo.RevParse(head)`
- the language
- `PromptVersion` (currently "v6")
- the backend name

If any of these changes, the key changes, there is no cached file, and `generate.Generate` runs. That covers new commits on head, a moved merge base, a different language or LLM, or a bumped `PromptVersion`. For the working tree, `RevParse` returns `worktreeID`, a hash of HEAD plus the diff, so any edit changes the key.

**Not shown:** The code never deletes or expires cached files. Answers to reader questions are saved back under the same key via `SaveCached` in `Asker.Save`.

## diff-tour-ask

How does asking a question during a tour work in diff-tour, from typing the question to the new steps appearing?

Points:
1. The viewer opens an ask panel (with suggested questions) and startAnswer runs generate.Answer in the background
2. Answer prepares the same workspace as a tour and also writes the whole tour as tour.json
3. askPrompt lists the tour steps with the current one marked, includes the current step's JSON and the reader's question
4. The reply goes through the same parse and repair loop (settle) and its steps are taken out of the returned chapters
5. The new steps are inserted right after the current step (InsertAfter), marked with the question, and saved to the cached tour when possible

### diff-tour-ask/056d

NOT SURE

### diff-tour-ask/7866

In `Viewer.Update`, pressing `?` or `/` calls `openAsk`, which (if an `Asker` exists) sets `askChoosing`. `updateAsk` handles navigation among the step's `Asks` suggestions and free-text input; Enter calls `startAnswer`.

`startAnswer` sets `askAnswering` and launches a goroutine running `generate.Answer`, passing progress through a channel (`waitFor`, `answerTick`). `Answer` calls `PrepareWorkspace`, `writeLessons`, writes `tour.json`, builds the prompt with `askPrompt`, runs `backend.Run`, then `settle` (parse and repair, via `parse`/`extractJSON`). It clears `Question`/`Asks` on the returned steps and sets `Asked` on the first.

On `answerDoneMsg`, `updateAsk` calls `Tour.InsertAfter`, refreshes `v.steps`, `goTo`s the next step, and calls `asker.Save` if set. `render.Frame` shows the `Asked` text.

Not shown: the prompt files (`prompts/ask.md`, `ask-output.md`) contents.

### diff-tour-ask/dc9d

NOT SURE

### diff-tour-ask/92d3

**Flow, as far as the packed content shows:**

1. `?` opens the panel via `Viewer.openAsk`, which offers `suggestions()` (the step's `Asks`) plus a free-text line. `updateAsk` handles the keys, and `askPanel` draws it.
2. Submitting calls `startAnswer(q)`, which uses the `Asker` (backend, `generate.Request`, `WorkDir`) and a cancelable context. Progress arrives as `answerProgressMsg`, and `answerTick` refreshes the spinner.
3. `generate.Answer` builds the prompt with `askPrompt` and returns one or two `tour.Step`s plus `Stats`. These come back as `answerDoneMsg`.
4. `tour.Tour.InsertAfter` puts the steps after the current one. `Asker.Save`, if set, persists the tour.
5. `render.Frame` shows the `Asked` question first on answer steps.

**Not shown:** the function bodies were compressed away. That covers how `Answer` runs the backend and parses the reply, and exactly where `answerDoneMsg` handling calls `InsertAfter` and `Save`.

### diff-tour-ask/9c00

In `Viewer.Update`, pressing `?` or `/` calls `openAsk`, which (if an `Asker` exists) sets `askChoosing`. Further keys and pastes go to `updateAsk`, where the reader picks one of the step's `suggestions()` (`Asks`) or types free text; Enter calls `startAnswer`.

`startAnswer` launches a goroutine running `generate.Answer`, streaming `answerProgressMsg` messages. `askPanel` shows progress, and Esc cancels.

`Answer` runs `PrepareWorkspace`, `writeLessons`, writes `tour.json`, builds the prompt with `askPrompt`, runs `backend.Run`, then `settle` parses and repairs the reply. It clears `Question`/`Asks` on the new steps and sets `Asked` on the first.

On `answerDoneMsg`, `updateAsk` calls `Tour.InsertAfter`, refreshes `v.steps`, moves with `goTo(index+1)`, shows `ask.added`, and calls `asker.Save` if set, which persists the tour via `SaveCached`.

The prompt files (`ask.md`, `ask-output.md`) aren't shown.

### diff-tour-ask/753d

Pressing `?` in the Viewer calls `openAsk`, which shows `askPanel` with the step's `suggestions()` (the step's `Asks`) plus a free-text line. `updateAsk` handles selection, typing, Enter and Esc. Enter calls `startAnswer(q)`, which sets the mode to `askAnswering`, creates a cancelable context and a progress channel, and runs `generate.Answer` in a goroutine. It also drives `answerTick` and `answerProgressMsg` for the progress display.

`Answer` builds its prompt with `askPrompt` from the `Question` (tour, step index, text). It prepares the workspace and calls the backend, which returns one or two `tour.Step`s. Then `answerDoneMsg` carries the steps and stats.

Not shown in the provided content: the handling of `answerDoneMsg` inside `Viewer.Update`, so exactly how `tour.InsertAfter` is called, the jump to the new step, and when `Asker.Save` runs. Also missing: `Answer`'s body details and the `ask` prompt template files.
