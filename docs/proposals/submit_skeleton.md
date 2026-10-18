# Proposal: one submit skeleton, thin backend hooks

Status: design proposal; drift items 1-4 and 7 are fixed (2 Oct), the skeleton itself is not built. Line numbers
are from the working tree on 2026-10-02 and will drift (core3 is being edited concurrently); function names are the
stable anchor.

## Problem

`GraphicsDeviceRef_submitCommands` (src/graphics/generic/device.c:2421-2715) validates, locks, bumps
`submitId`/`fifId` (2641-2642), builds `CBufferData`, then hands the WHOLE frame to one backend call,
`GraphicsDevice_submitCommandsExt` (2663; table slot `deviceSubmitCommands`, include/graphics/generic/interface.h:418).
Each backend then re-implements the same ~450-line orchestration:

- D3D12: `D3D12GraphicsDevice_submitCommands`, src/graphics/d3d12/generic/dx_device.c:1147-1638, flush 1640-1716
- Vulkan: `VkGraphicsDevice_submitCommands`, src/graphics/vulkan/generic/vk_device.c:1874-2469, flush 2471-2571

Every cross-cutting feature lands twice (breadcrumbs, timestamps, loss detection, the flush log line), and the
copies have already drifted in ways that are bugs, not style (see "Drift found while reading").

## 1. The phases today

Legend: **I** identical logic, **S** same shape / different API calls, **D** genuinely different.

| # | Phase | D3D12 (dx_device.c) | Vulkan (vk_device.c) | Kind |
|---|---|---|---|---|
| 0 | Scratch reservation (present lists) | - | 1897-1916 | D (Vulkan WSI arrays) |
| 1 | Frame-slot wait | 1175-1190: `++fenceId`, wait `fenceId - framesInFlight` on the timeline fence, `INFINITE` | 1918-1952: wait `commitFence[fifId]` if `commitFencePending`, 1 s slices, then reset fence | S (timeline vs binary fence) |
| 2 | Retire API objects of the slot | - (rides `resourcesInFlight`) | 1954-1971: `retiredAs`, `retiredSwapchains` | D (Vulkan only) |
| 3 | Read previous slot's timestamps | 1325-1336: mapped readback buffer; only when there are command lists | 1974-2010: `vkGetQueryPoolResults`, mask valid bits, sets `lost` | S |
| 4 | Swapchain acquire | - (ring advanced at present, 1568-1569) | 2012-2064: `vkAcquireNextImageKHR`, wait semaphores/stages, `lastSubmitId` stamp | D |
| 5 | Per-frame cbuffer fill | 1192-1212 | 2066-2105 (+ `vkFlushMappedMemoryRanges` if incoherent) | I (+ one mapped-flush call) |
| 6 | Command allocator lookup, lazy create, debug name, reset | 1220-1302 | 2116-2224 | S |
| 7 | Breadcrumbs start + uploads (`handleNextFrame`) + breadcrumb end | 1311-1320 | 2230-2239 | I (marker stage enum differs) |
| 8 | `buildTimings`, grow query heap/pool | 1338-1342, 1364-1419 (+ readback buffer) | 2241-2284, reset pool 2281-2284 | S |
| 9 | Size DispatchRaysIndirect args | 1344-1362 | - | D (D3D12 only) |
| 10 | Frame cbuffer barrier | 1421-1448 (only if any op) | 2286-2306 (always) | S |
| 11 | Op walk | 1453-1467 via `CommandList_processExt` | 2311-2325 via `VK_WRAP_FUNC(CommandList_process)` | I |
| 12 | Readback breadcrumb + `flushPendingPulls` | 1469-1476 | 2327-2334 | I |
| 13 | Timestamp resolve into readback | 1478-1485 `ResolveQueryData` | - (host read in phase 3) | D |
| 14 | Swapchains to PRESENT + in-flight push | 1487-1529 | 2336-2382 | S |
| 15 | Close / end | 1531-1534 | 2384-2386 | S |
| 16 | Execute | 1537-1541 `ExecuteCommandLists` | 2389-2415 `vkQueueSubmit` + fence + semaphores, sets `commitFencePending` | S |
| 17 | Present | 1543-1570 `Present1`, ring++ | 2417-2459 `vkQueuePresentKHR`, SUBOPTIMAL -> `requireResize` | S |
| 18 | Signal | 1572-1581 `Signal(fenceId)`, swapchain `lastFenceId` | (fence passed to submit) | S |
| 19 | Failure unwinding | 1583-1606: drop poisoned list, CPU- or GPU-signal the fence | 2461-2468: clears scratch only | D (and see drift) |
| 20 | Post diagnostics | 1585-1586 info-queue drain, 1608-1630 NvAPI RT validation flush | - | D |
| 21 | Flush (mid-recording split) | 1640-1716 | 2471-2571 | S (log text I) |
| 22 | Wait (device idle) | 922-925 -> `DxGraphicsDevice_waitFence` 752-825 | 1603-1652 | S |
| 23 | `getCommandAllocator` | 927-945: `>= 3` queues, `>= 3` frames, stride 3 | 1654-1678: `resolvedQueues`, `fifCount` | S (diverged) |

Generic pieces the backends already call: `GraphicsDeviceRef_handleNextFrame` (device.c:1773-1835: retires the
slot's `resourcesInFlight`, frees `stagingAllocations[fifId]`, completes pulls, calls `bufferFlush`/`textureFlush`
per pending resource), `GraphicsDeviceRef_flushPendingPulls` (2253-2389), `GraphicsDevice_startBreadcrumbs` /
`claimBreadcrumb` (device_loss.c:326-375), `GraphicsDevice_buildTimings` / `resolveTimings`, and
`GraphicsDeviceRef_wait` (device.c:2779) which wraps `GraphicsDeviceRef_waitExt`.

Roughly: of ~450 lines per backend, phases 5, 7, 11, 12 are identical (~80 lines), 1, 3, 6, 8, 10, 14-18, 21-23 are
the same skeleton around a handful of API calls (~300), and only 0, 2, 4, 9, 13, 20 are truly backend-shaped.

### Drift found while reading (fix regardless of this proposal)

1. **Vulkan hangs after a recording failure.** *Fixed 2 Oct: the flag clears with the reset and a failed frame submits empty.* Phase 1 resets `commitFence[fifId]` (1952) but leaves
   `commitFencePending[fifId]` true; only `vkQueueSubmit` (2414) rewrites it. Any failure between 1952 and 2413
   (allocator, `handleNextFrame`, an op, pulls, end) leaves "pending" on an unsignaled fence, so the next submit at
   that slot and every `GraphicsDeviceRef_wait` (1626-1628) wait forever (in 1 s slices). D3D12 handles the same
   case by signalling from the CPU (1600-1606).
2. **D3D12's frame-slot wait ignores device removal.** *Fixed 2 Oct: it is `DxGraphicsDevice_waitFence`.* 1181-1189 is `WaitForSingleObject(INFINITE)`, while
   `DxGraphicsDevice_waitFence` (765-817) checks `GetCompletedValue() == U64_MAX`, slices, and asks
   `GetDeviceRemovedReason`. The frame path should be the latter.
3. **D3D12 fence values are not submit ids.** *Fixed 2 Oct: per slot fence values, swapchains track `lastSubmitId`. It was unsafe too: the submit re-signalled the flush's already completed value, so waits passed early.* `fenceId` is bumped by both submit (1177) and flush (1672), but the
   slot wait computes `fenceId - framesInFlight` as if one value per submit. After a flush this waits on a LATER
   value than the slot's own (over-conservative, not unsafe), and `lastFenceId` (1576) is not comparable to
   `completedSubmitId`, which is why Vulkan swapchains track `lastSubmitId` (2034) and D3D12 ones a fence value.
4. **D3D12 with swapchains but no command lists** *(fixed 2 Oct: nothing executes)* (allowed by device.c:2441): `commandBuffer` stays NULL and
   1540 executes a count of 1 with a NULL list, and nothing transitions the back buffer to PRESENT. Vulkan submits
   zero command buffers (2406). Worth a test (`test_graphics_virtual_swapchain` drives owned images only).
5. **Timestamp read differs by condition.** D3D12 reads only inside `if(commandLists...)` (1325); Vulkan always
   (1978). A swapchain-only frame on D3D12 keeps stale `timings`.
6. **Allocator sizing.** D3D12 allocates `3 * threads * 3` (dx_device.c:443) and indexes with stride 3; Vulkan
   `framesInFlight * threads * resolvedQueues` (vk_device.c:1103). Both re-read `Platform_getThreads()` at lookup,
   which is the "thread count changes at runtime" hole their comments mention.
7. **Flush marks the in-progress submit complete.** Both flushes call the public `GraphicsDeviceRef_wait`
   (dx 1682, vk 2510), which sets `completedSubmitId = submitId` (device.c:2810) while `submitId` was already
   bumped for the frame being recorded (2642). Fixed 2 Oct (`GraphicsDeviceRef_waitFlush`, `submitId - 1`); the
   skeleton below makes the correct value fall out structurally.
8. **No interface test exercises the flush.** Nothing in src/graphics/test/interface lowers `flushThreshold`;
   the split path (phase 21) only runs on large uploads.

## 2. Proposed skeleton

### Where state lives

Generic (`GraphicsDevice`, device.h:192-341) gains what both backends keep separately today:

```c
typedef struct GraphicsQueueTimeline {
	U64 nextValue;                          //Last value handed out; monotonic, one per execute (not per submit)
	U64 slotValue[MAX_FRAMES_IN_FLIGHT];    //Value whose completion frees slot fifId (0 = nothing pending)
	U64 completedValue;                     //Highest value known complete (cached)
} GraphicsQueueTimeline;

//in GraphicsDevice:
GraphicsQueueTimeline timeline[EGraphicsQueue_Count];   //Graphics only to start
U64 slotSubmitId[MAX_FRAMES_IN_FLIGHT];                  //submitId that last used the slot
U32 commandAllocatorThreads;                             //Captured at create, not re-read per lookup
U8 resolvedQueues;
```

Ext keeps API objects only: D3D12 `ID3D12Fence` (already a timeline), Vulkan `commitFence[fif]` plus the value
each fence carries (until it moves to a timeline semaphore, see open questions), query heaps/pools, WSI
semaphores, allocator pools. `DxGraphicsDevice::fenceId` and `VkGraphicsDevice::commitFencePending` disappear: the
first becomes `timeline.nextValue`, the second becomes `slotValue[fif] != 0`.

The submit context lives on the stack of the generic submit and is what every hook receives:

```c
typedef enum ESubmitStage {
	ESubmitStage_None, ESubmitStage_SlotFree, ESubmitStage_Acquired,
	ESubmitStage_Recording, ESubmitStage_Executed, ESubmitStage_Presented
} ESubmitStage;

typedef struct GraphicsSubmit {
	GraphicsDeviceRef *deviceRef;
	const ListCommandListRef *lists;
	const ListSwapchainRef *swapchains;
	U8 fifId, queue, stage, isSplit;
	U32 chunk;                  //Split index within this submit; 0 for the first
	U64 signalValue;            //Value the current execute signals
	void *cmd;                  //Backend recording state (DxCommandBufferState / VkCommandBufferState)
} GraphicsSubmit;
```

### Skeleton (device.c, replaces the call at 2663)

```c
//after validation, locking, fifId/submitId bump and CBufferData (unchanged, 2436-2658)

GraphicsSubmit s = { deviceRef, lists, swapchains, .fifId = device->fifId };

//1. Slot reuse: one wait, one retire, one breadcrumb restart, one timestamp read
gotoIfError3(clean, Graphics_waitValue(&s, device->timeline[q].slotValue[fif]));   //hook: waitValue
s.stage = ESubmitStage_SlotFree;
GraphicsDevice_retireSlot(device, fif);           //generic: resourcesInFlight, staging, pulls (from handleNextFrame)
retireSlotExt(&s);                                //hook: Vulkan retiredAs/retiredSwapchains, D3D12 no-op
if(timings && slotSubmitId[fif])                  //same condition on both APIs
	if(readTimestampsExt(&s, ticks, timingSlots[fif]))   //hook: host read of resolved ticks
		GraphicsDevice_resolveTimings(...);

//2. Swapchains
gotoIfError3(clean, acquireExt(&s, e_rr));        //hook: Vulkan acquire + wait semaphores, D3D12 no-op
s.stage = ESubmitStage_Acquired;
GraphicsDevice_fillFrameData(device, swapchains, data);   //generic memcpy into mapped frameData[fif]
flushMappedExt(device, frameData[fif], 0, sizeof(CBufferData));   //hook (existing allocator concern)

//3. Record
if(lists && lists->length) {
	gotoIfError3(clean, beginRecordingExt(&s, threadId, e_rr));   //hook: allocator lookup/create/name/reset/begin
	s.stage = ESubmitStage_Recording;
	GraphicsDevice_startBreadcrumbs(device);
	timingSlots[fif] = buildTimings(...);
	gotoIfError3(clean, prepareFrameExt(&s, e_rr));   //hook: grow query storage, reset pool, D3D12 DRI args,
	                                                  //      frame cbuffer barrier
	breadcrumbScope(&s, GRAPHICS_BREADCRUMB_UPLOADS, recordUploads);   //handleNextFrame's upload half
	for each list, for each op: CommandList_processExt(...);           //moved verbatim from both backends
	breadcrumbScope(&s, GRAPHICS_BREADCRUMB_READBACKS, GraphicsDeviceRef_flushPendingPulls);
	gotoIfError3(clean, endRecordingExt(&s, e_rr));   //hook: resolve queries (D3D12), swapchains -> PRESENT, close
}

//4. Execute, present, signal
s.signalValue = ++device->timeline[q].nextValue;
gotoIfError3(clean, executeExt(&s, e_rr));        //hook: submit + signal signalValue (+ WSI semaphores)
s.stage = ESubmitStage_Executed;
device->timeline[q].slotValue[fif] = s.signalValue;
device->slotSubmitId[fif] = device->submitId;
push swapchains to resourcesInFlight[fif]; swapchain->lastSubmitId = submitId;   //one notion, both APIs
gotoIfError3(clean, presentExt(&s, e_rr));        //hook: Present1 / vkQueuePresentKHR, ring, requireResize
s.stage = ESubmitStage_Presented;

clean:
if(!s_uccess) GraphicsSubmit_unwind(&s);          //below
diagnosticsExt(&s, s_uccess);                     //hook: D3D12 info queue drain + NvAPI RT flush; Vulkan no-op
```

`breadcrumbScope` is the claim/marker-in/body/marker-out pattern of phases 7 and 12, with the marker written by
the existing `DxGraphicsDevice_breadcrumb` / `VkGraphicsDevice_breadcrumb` behind one `writeBreadcrumb` hook
(`begin`/`end` instead of the API stage enum).

### Hooks (`GraphicsInterfaceTable`, replacing `deviceSubmitCommands`)

| Hook | Signature | Owns |
|---|---|---|
| `waitValue` | `Bool (GraphicsSubmit*, U8 queue, U64 value, Error*)` | Block until `value` completes, 1 s slices, stall log, loss check. D3D12: `DxGraphicsDevice_waitFence`. Vulkan: map value to the fif fence that carries it, wait, reset. Also used by `GraphicsDeviceRef_wait` (wait on `nextValue`) |
| `retireSlot` | `void (GraphicsSubmit*)` | API handles deferred until the slot is free (Vulkan AS and swapchains) |
| `readTimestamps` | `Bool (GraphicsSubmit*, U64 *ticks, U32 count)` | Host read of the slot's ticks, already masked to valid bits |
| `acquire` | `Bool (GraphicsSubmit*, Error*)` | Next image per presented swapchain; owned-image ring advance moves to generic |
| `flushMapped` | `Bool (GraphicsDevice*, DeviceBuffer*, U64 off, U64 len, Error*)` | Non-coherent flush; D3D12 no-op. Also wanted by the upload paths (vk_device_buffer.c:449) |
| `beginRecording` | `Bool (GraphicsSubmit*, U32 threadId, Error*)` | Allocator at generic index (below), lazy create + debug name, reset, begin; fills `s->cmd` |
| `prepareFrame` | `Bool (GraphicsSubmit*, Error*)` | Query storage growth and reset, D3D12 DRI reservation, frame cbuffer barrier |
| `writeBreadcrumb` | `void (GraphicsSubmit*, U32 slot, Bool end)` | One marker write |
| `endRecording` | `Bool (GraphicsSubmit*, Bool forSplit, Error*)` | D3D12 `ResolveQueryData`, swapchain PRESENT barriers (skipped when `forSplit`), close |
| `execute` | `Bool (GraphicsSubmit*, Error*)` | Submit `s->cmd` (or nothing), signal `s->signalValue`; Vulkan wires acquire/present semaphores only on the final chunk |
| `present` | `Bool (GraphicsSubmit*, Error*)` | Present the acquired images, SUBOPTIMAL -> `requireResize` |
| `abandon` | `void (GraphicsSubmit*)` | Drop a list poisoned mid-recording (D3D12 1591-1595); Vulkan resets the buffer |
| `resetRecordingState` | `void (void *cmd)` | The bound-state tracker reset both flushes do by hand (dx 1694-1708, vk 2547-2567) |
| `diagnostics` | `void (GraphicsSubmit*, Bool ok)` | D3D12 info queue drain + NvAPI flush |

`CommandList_processExt`, `bufferFlush`, `textureFlush`, `bufferPull`, `texturePull` and the RTAS flush hooks
stay as they are. Fourteen small hooks replace one huge one; each is 10-80 lines of what is in the backends now.

### Command allocators

One generic index, one sizing rule, both from fields captured at create:

```c
U64 GraphicsDevice_commandAllocatorIndex(const GraphicsDevice *d, U8 resolvedQueue, U32 thread, U8 fif) {
	if(resolvedQueue >= d->resolvedQueues || thread >= d->commandAllocatorThreads || fif >= d->framesInFlight)
		return U64_MAX;
	return resolvedQueue + ((U64) fif * d->commandAllocatorThreads + thread) * d->resolvedQueues;
}
```

Backends keep their typed `ListDxCommandAllocator` / `ListVkCommandAllocator`, sized to
`resolvedQueues * framesInFlight * commandAllocatorThreads` and indexed only through this. D3D12 sets
`resolvedQueues` from its `queues[]` the way Vulkan does; that removes both hardcoded 3s (dx_device.c:443, 936).
A chunk dimension is added later for asynchronous splits (section 3).

### Errors, unwinding, loss: once

`GraphicsSubmit_unwind` is keyed on `s.stage` only:

- `Recording`: `abandon` (D3D12 drops the list; Vulkan resets it).
- `SlotFree` .. `Recording` (nothing executed): run `execute` with no command buffer. On D3D12 that is the
  queue signal of the slot's value; on Vulkan an empty `vkQueueSubmit` that waits the acquire semaphores and
  signals the fence. Both leave the slot pending on a value that WILL complete, so the next wait cannot hang
  (fixes drift 1) and a Vulkan acquire semaphore is never left signalled. Whether an acquired-but-unpresented
  image must still be presented is open question 3.
- `Executed` but present failed: nothing to undo; the value is already pending.

Loss reporting stays where it is (`GraphicsDeviceRef_reportLoss` at device.c:2700 and 2802), but every hook
returns through one generic check, so "the API said lost" is recorded in generic state rather than in
`VkGraphicsDevice::lost` (set ad hoc at 1994) and checked differently per backend. `waitValue` being the only
wait means the frame-slot wait gets the removal check D3D12 lacks (drift 2).

## 3. Roadmap fit

**Flush becomes a split, not an idle.** `GraphicsSubmit_split(&s, reason)` replaces both `*GraphicsDevice_flush`:
`endRecording(forSplit)` -> `execute` with `++nextValue` -> `waitValue` on THAT value only (not
`vkDeviceWaitIdle`/the global wait) -> free `stagingAllocations[fif]` (the reason for the wait: the per-slot staging
ring is the resource being reclaimed) -> `beginRecording` on the same allocator -> `resetRecordingState`. The
throttled log line (dx 1644-1653, vk 2478-2487) is written once. Because one queue completes in order, waiting on
the split's value proves every earlier submit complete, so the split sets
`completedSubmitId = submitId - 1` and never `submitId` (drift 7 by construction). The `inRender` refusal
(dx 1658) becomes a generic check for both APIs (Vulkan lacks it today). The Vulkan fence borrow/reset dance
(2512-2519) goes away: the split does not touch the slot fence in the timeline model.

**Upload batching.** Today each pending resource records its own barrier + copy (`DeviceBufferRef_flush`,
vk_device_buffer.c:449-480 and 595-620; dx_device_buffer.c:532-536 and 650-654; textures likewise). With the
skeleton owning the upload phase, generic code walks `pendingResources` once, allocates staging, fills it (the
JobQueue fan-out of roadmap.md:176-201 joins here, before recording), and hands the backend ONE batch:
`recordUploads(GraphicsSubmit*, const GraphicsUploadBatch*)` with all destination transitions in one barrier
group, then all copies, staging transition once. The flush threshold check moves out of six backend files into
the batch builder, which can split the batch at the threshold instead of mid-resource.

**Submit splitting for TDR budgets.** The same `split` without the wait: chunks signal
increasing values on one queue and the slot's value is the last one. That needs an allocator per chunk (the
chunk dimension above, grown on demand) since a chunk's allocator cannot reset while the next is recording.
Split points are scope boundaries (roadmap.md:70-72); the skeleton's op walk is where a "budget exceeded at scope
end" test goes, once, for both APIs.

**More queues / async compute.** `timeline[queue]` per resolved queue; a scope assigned to compute records into
a second `GraphicsSubmit` and cross-queue dependencies become "wait queue A value N" on `execute`. On D3D12 this
is `ID3D12CommandQueue::Wait` on a fence; on Vulkan it requires timeline semaphores (binary fences cannot be waited
on by another queue). Gated, as the roadmap says, on dependencies actually synchronizing (roadmap.md:49-53, 73-74).

**Multi-threaded encoding.** Phase order in the skeleton is already resolve-then-encode friendly: a scope pass
(single thread) resolves barriers from registered accesses (roadmap.md:41-48), then ranges of scopes are encoded
as jobs on the lent `JobQueue`, each calling `beginRecording(threadId)` (the allocator thread dimension finally
used), and `execute` takes the list of command buffers in order. Nothing in the hooks assumes one `cmd`; `s.cmd`
becomes `cmd[threadCount]`. Breadcrumb claims must then be pre-assigned in the resolve pass (claim is not
thread safe, device_loss.c:326).

**Metal.** Maps cleanly: `beginRecording` = `[queue commandBuffer]` (no allocator; index unused), timeline =
`MTLSharedEvent` (`waitValue` = `waitUntilSignaledValue`), `acquire` = `nextDrawable`, `readTimestamps` = counter
sample buffer resolve. One wrinkle: Metal presents by encoding `presentDrawable` BEFORE `commit`, so its
`execute` presents and its `present` is a no-op; the skeleton allows that since both run back to back.

## 4. Migration (each step green on the interface suite, D3D12 and Vulkan, plus WARP and lavapipe)

0. **Test first.** Add an interface test that lowers `flushThreshold` / `flushThresholdPrimitives` so uploads and a
   BLAS build split mid-submit, and one swapchain-only frame on a real (non-owned) swapchain if CI can create one.
   Without these, steps 6-7 move untested code.
1. **Fix drift 1, 2, 4, 5 in place.** Small, backend-local, independently reviewable; makes the later moves
   behaviour-preserving rather than behaviour-changing.
2. **Generic allocator index** + `resolvedQueues`/`commandAllocatorThreads` on `GraphicsDevice`; both
   `getCommandAllocator`s become two-line wrappers. Risk: D3D12 pool count shrinks from 9x to
   `resolvedQueues * framesInFlight` x threads; the pool free loop (dx_device.c:650) must use the same count.
3. **Timeline bookkeeping.** Introduce `GraphicsQueueTimeline` and `waitValue`; D3D12 `fenceId` -> `nextValue`
   (fixes drift 3), Vulkan keeps binary fences but records which value each fif fence carries. Point
   `GraphicsDeviceRef_waitExt` at `waitValue(nextValue)`. Risk: Vulkan's "value -> fence" mapping is only exact
   because there is one execute per slot; splits (step 7) execute without the slot fence, so they must wait on the
   fence of the CURRENT slot or move to a timeline semaphore first (open question 1).
4. **Pull slot reuse into generic:** `retireSlot`, `readTimestamps`, `flushMapped`, cbuffer fill, owned-image
   ring advance, `acquire`. Backend `submitCommands` shrinks from the top. Run `test_graphics_timestamps`,
   `test_graphics_virtual_swapchain`, `test_graphics_formats_frames`.
5. **Move the skeleton.** Add `beginRecording`, `prepareFrame`, `writeBreadcrumb`, `endRecording`, `execute`,
   `present`, `diagnostics`; generic owns phases 7, 11, 12. Delete `deviceSubmitCommands` from the table. This is
   the big diff but mostly moves; review per hook. Watch: `GraphicsDevice_rebindDescriptors` and the lazy
   descriptor bind stay backend-internal; `handleNextFrame` splits into `retireSlot` (generic) and the upload half.
   Static-linking builds call `*Ext` directly (interface.c `#else`), so every new hook needs both the table entry
   and the `DX_WRAP_FUNC`/`VK_WRAP_FUNC` naming; remember the build-skew lore (roadmap.md:383-386) when the table
   layout changes: clean-build both backend DLLs.
6. **Unwinding by stage**, `abandon`, empty execute on failure. Test by injecting a failure (an op that errors) and
   submitting again on the same slot.
7. **Flush -> `GraphicsSubmit_split`.** Both `*GraphicsDevice_flush` and their six call sites
   (dx/vk `_device_buffer.c`, `_device_texture.c`, `_blas.c`) call the generic split through the context, which
   carries the `submitId - 1` rule `GraphicsDeviceRef_waitFlush` applies today.
8. **Upload batch hook.** Generic batch builder; per-resource `bufferFlush`/`textureFlush` become the fallback and
   then go. Measure on the 533 MB upload from roadmap.md:133-137.

Later, not in this series: chunk allocators and budget splits, Vulkan timeline semaphores, queue timelines,
threaded encoding.

## 5. Open questions

1. **Vulkan timeline semaphores.** Core in 1.2 and required for cross-queue waits; does every target (Android
   gfxstream, lavapipe, mobile) expose them reliably? If yes, step 3 should switch and drop per-fif fences, keeping
   binary semaphores only for WSI.
2. **Split semantics.** Should a threshold split keep waiting (today's memory-reclaim behaviour) or grow the staging
   ring and never wait? The second is cheaper on the GPU timeline and costs memory.
3. **Unwinding with an acquired image.** On a failed Vulkan frame after acquire, present the (unrendered) image to
   keep the swapchain balanced, or leak the acquisition and recreate? D3D12 has no equivalent.
4. **Swapchain-only frames on D3D12.** Is "swapchains, no command lists" a supported case (present last image
   again), or should generic validation refuse it?
5. **`CBufferData` in the skeleton.** It is tagged "replace this entirely" (device.h:49); should the skeleton
   treat the frame cbuffer as just another generic upload so `prepareFrame` loses the barrier?
6. **Hook granularity.** Fourteen hooks in the table vs. a per-backend `GraphicsSubmitVTable` stored on the device
   ext (no interface.h churn for future hooks, one indirection less in static builds). Preference?
7. **Diagnostics placement.** NvAPI RT validation flush (dx 1608-1630) runs on every submit even on success;
   keep it per submit, or only on failure / every N submits?
