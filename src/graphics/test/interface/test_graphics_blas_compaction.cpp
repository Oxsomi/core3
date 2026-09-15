/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

//graphics/test/interface/test_graphics_blas_compaction.cpp

#include "test_graphics_shared.hpp"

#include "types/container/log.hpp"

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/list_basic_types.h"
	#include "types/test/test.h"
	#include "platforms/platform.h"
	#include "graphics/generic/blas.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/graphics_types.h"
	#include "graphics/generic/instance.h"
	#include "graphics/generic/tlas.h"
	#include "test_graphics_shared.h"
} }

using namespace oxc;

// -- 65. BLAS compaction: every way of getting it wrong is made loud -------

//Compaction MOVES a structure, so getting it wrong does not fault: a TLAS keeps the address of memory that
//has been freed, the scene quietly loses that geometry, and the frame gets FASTER. Nothing reports it.
//
//What this module proves is that each of those turns into something visible. A call that cannot be legal
//is refused outright; a caller who still owes a TLAS update is refused at submit; and where compaction IS
//legal, including on a structure a TLAS has already adopted, it succeeds and the TLAS re-resolves.

namespace {

	//One triangle compacts to nothing worth measuring: drivers round a small structure up to the same
	//allocation granularity and report no saving at all, so a size assertion on it would be testing the
	//driver's rounding. A soup is used instead, sized so the saving is real but the build stays quick.

	constexpr c::U32 TRIANGLE_COUNT = 4096;
	constexpr c::U32 VERTEX_COUNT = TRIANGLE_COUNT * 3;

	//Query slots return to a free list when a compaction consumes them, so the interesting number is how
	//many are outstanding AT ONCE. Building every structure before compacting any of them holds this many
	//slots simultaneously, which is what forces a second pool to exist.

	constexpr c::U32 CHUNK_CROSSING_COUNT = 257;

	//A deterministic spread, so every triangle occupies its own region and the structure has real depth to
	//compact rather than collapsing to one node.

	void fillSoup(c::F32 *out, c::U32 triangles) {

		c::U32 seed = 0x9E3779B9u;

		for (c::U32 i = 0; i < triangles; ++i) {

			c::F32 base[3];

			for (c::U32 axis = 0; axis < 3; ++axis) {
				seed = seed * 1664525u + 1013904223u;
				base[axis] = (c::F32)(seed >> 8) / (c::F32)(1u << 24) * 64;
			}

			for (c::U32 v = 0; v < 3; ++v) {

				c::F32 *vert = out + ((c::U64)i * 3 + v) * 4;

				vert[0] = base[0] + (v == 1 ? 1.0f : 0.0f);
				vert[1] = base[1] + (v == 2 ? 1.0f : 0.0f);
				vert[2] = base[2];
				vert[3] = 1;
			}
		}
	}

	//Record one BLAS build and nothing else. Kept separate from the submit so a case can stop between the
	//two, which is exactly the state the timing refusal is about.

	[[nodiscard]] c::Bool recordBuild(
		c::Test *t, gfx::Device &dev, gfx::CommandList &list, gfx::Blas &blas, const c::C8 *name
	) noexcept {

		if (!c::Test_assert(t, name, dev.createCommandList(c::KIBI, 32, 32, list, true, &t->err)))
			return false;

		if (!c::Test_assert(t, name, list.begin(true, &t->err)))
			return false;

		{
			gfx::CommandScope scope = list.scope({}, 0, {}, &t->err);

			if (!c::Test_assert(t, name, (c::Bool) scope))
				return false;

			if (!c::Test_assert(t, name, scope.updateBlas(blas, &t->err)))
				return false;
		}

		return c::Test_assert(t, name, list.end(&t->err));
	}

	//Records a compaction and runs it. The copy is a GPU op, so "did it compact" is answered after the
	//submit, not by the record call.

	[[nodiscard]] c::Bool compactAndRun(
		c::Test *t, gfx::Device &dev, gfx::Blas &blas, const c::C8 *name
	) noexcept {

		gfx::CommandList list;

		if (
			!c::Test_assert(t, name, dev.createCommandList(c::KIBI, 32, 32, list, true, &t->err)) ||
			!c::Test_assert(t, name, list.begin(true, &t->err))
		)
			return false;

		{
			gfx::CommandScope scope = list.scope({}, 0, {}, &t->err);

			if (
				!c::Test_assert(t, name, (c::Bool) scope) ||
				!c::Test_assert(t, name, scope.compactBlas(blas, &t->err))
			)
				return false;
		}

		return c::Test_assert(t, name, list.end(&t->err)) && gfxtest::submitAndWait(t, dev, list);
	}

	//Only ATTEMPTS the record, on a list of its own because a rejected record invalidates the recording it
	//was made into. e_rr stays null: the refusal is the expected result, not a failure to note.

	[[nodiscard]] c::Bool tryRecordCompact(gfx::Device &dev, gfx::Blas &blas) noexcept {

		gfx::CommandList list;

		if (!dev.createCommandList(c::KIBI, 32, 32, list, true, nullptr) || !list.begin(true, nullptr))
			return false;

		gfx::CommandScope scope = list.scope({}, 0, {}, nullptr);
		return scope && scope.compactBlas(blas, nullptr);
	}

	[[nodiscard]] c::U64 asSize(const gfx::Blas &blas) noexcept {
		const c::BLAS *dat = blas.data();
		return dat && dat->base.asBuffer ? c::bufferOf(dat->base.asBuffer)->resource.size : 0;
	}
}

extern "C" void Test_graphicsBlasCompaction(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "BLAS/compaction");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	//Compaction is an acceleration structure operation; it needs neither a ray pipeline nor ray queries.

	if (!(dev.info().capabilities.features & c::EGraphicsFeatures_Raytracing)) {
		c::Test_print(t, "Device lacks raytracing, skipping BLAS compaction tests");
		return;
	}

	//Rebinds dev on the backend that needs a private device for GPU based validation. Every handle below is
	// declared after it so all of them are released before its teardown runs.

	gfxtest::RtDedicatedDevice dedicated(t, dev);

	if (!dedicated)
		return;

	const c::Allocator *alloc = dev.alloc();

	gfx::OwnedList<c::Buffer> soup(alloc);

	if (!c::Test_assert(t, "allocSoup", c::Buffer_createUninitializedBytes(
		(c::U64) VERTEX_COUNT * 4 * sizeof(c::F32), alloc, &soup.list, e_rr
	)))
		return;

	fillSoup((c::F32*) soup.list.ptrNonConst, TRIANGLE_COUNT);

	gfx::DeviceBuffer positions;

	if (!c::Test_assert(t, "createPositions", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"Compaction positions", &soup.list, positions, nullptr, e_rr
	)))
		return;

	const c::BLASCreateInfo compactable = c::BLASCreateInfo_unindexed(
		c::ERTASBuildFlags_AllowCompaction, c::EBLASFlag_None,
		c::ETextureFormatId_RGBA32f, 0, 16, positions.region()
	);

	// -- The happy path, which is also the ONLY ordering that works ----------

	{
		gfx::Blas blas;
		gfx::CommandList list;

		if (
			c::Test_assert(t, "createBlas", dev.createBlas(compactable, "Compaction BLAS", blas, e_rr)) &&
			recordBuild(t, dev, list, blas, "recordBuild") &&
			gfxtest::submitAndWait(t, dev, list)
		) {

			const c::U64 before = asSize(blas);

			if (compactAndRun(t, dev, blas, "compact")) {

				const c::U64 after = asSize(blas);

				//A driver is allowed to report no saving, so the contract is that it never GROWS. The flag
				// is the part that must always hold: it is what stops a second copy from running.

				c::Test_assert(t, "compactNeverGrows", after && after <= before);
				c::Test_assert(t, "compactMarked", blas.data()->base.isCompacted);

				//The query slot went back on consumption; holding it would leak one slot per structure.

				c::Test_assert(t, "compactReleasedQuery", blas.data()->base.compactionQuery == c::U32_MAX);

				if (after < before)
					c::Test_print(t, "BLAS compaction saved memory as expected");

				//Compacting again is a no-op that REPORTS SUCCESS: a caller sweeping everything it owns
				// should not have to track which structures it already did.

				c::Test_assert(t, "compactTwiceIsNoOp", compactAndRun(t, dev, blas, "compactTwice"));
			}

			//The structure still works afterwards, and a TLAS built now picks up the NEW address.

			const c::TLASInstance instance = {
				.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
				.data = {
					.instanceId24_mask8 = 0xFFu << 24,
					.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_Default << 24,
					.blasCpu = blas.handle()
				}
			};

			gfx::Tlas tlas;

			if (c::Test_assert(t, "createTlasAfterCompact", dev.createTlas(
				c::ERTASBuildFlags_DefaultTLAS, &instance, 1, "Compaction TLAS", tlas, true, e_rr
			))) {

				gfx::CommandList tlasList;

				if (
					c::Test_assert(t, "createTlasList", dev.createCommandList(c::KIBI, 32, 32, tlasList, true, e_rr)) &&
					c::Test_assert(t, "beginTlasList", tlasList.begin(true, e_rr))
				) {
					{
						gfx::CommandScope scope = tlasList.scope({}, 0, {}, e_rr);

						if (c::Test_assert(t, "scopeTlas", (c::Bool) scope))
							c::Test_assert(t, "updateTlas", scope.updateTlas(tlas, e_rr));
					}

					if (c::Test_assert(t, "endTlasList", tlasList.end(e_rr)))
						c::Test_assert(t, "buildTlasOnCompacted", gfxtest::submitAndWait(t, dev, tlasList));
				}
			}
		}
	}

	// -- Compacting under a TLAS, and the submit that owes it an update ------

	//The one that matters. A structure a TLAS resolved can still be compacted, but the TLAS is then holding
	// an address that names released memory, so the submit after the copy is refused until it is updated.
	// e_rr is deliberately null on the refused calls: the error is the expected result, not a failure.

	{
		gfx::Blas blas;
		gfx::CommandList list;

		if (
			c::Test_assert(t, "createBlasPublish", dev.createBlas(compactable, "Publish BLAS", blas, e_rr)) &&
			recordBuild(t, dev, list, blas, "recordBuildPublish") &&
			gfxtest::submitAndWait(t, dev, list)
		) {

			const c::TLASInstance instance = {
				.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
				.data = {
					.instanceId24_mask8 = 0xFFu << 24,
					.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_Default << 24,
					.blasCpu = blas.handle()
				}
			};

			gfx::Tlas tlas;

			//AllowUpdate, because staleness has to be RECOVERABLE: a completed TLAS without it can never
			// re-resolve (its flush early outs forever), which is why compactBLASExt refuses that shape
			// outright rather than stranding it.

			gfx::CommandList adoptList;

			if (
				c::Test_assert(t, "createTlasBeforeCompact", dev.createTlas(
					(c::ERTASBuildFlags) (c::ERTASBuildFlags_DefaultTLAS | c::ERTASBuildFlags_AllowUpdate),
					&instance, 1, "Publish TLAS", tlas, true, e_rr
				)) &&

				//The TLAS has to be BUILT before the compact, or its first build rides the compact submit,
				// resolves the moved address after the adopt and legitimately clears the mark: adoption
				// means a completed structure, not a pending one.

				c::Test_assert(t, "createAdoptList", dev.createCommandList(c::KIBI, 32, 32, adoptList, true, e_rr)) &&
				c::Test_assert(t, "beginAdoptList", adoptList.begin(true, e_rr)) &&
				c::Test_assert(t, "endAdoptList", adoptList.end(e_rr)) &&
				gfxtest::submitAndWait(t, dev, adoptList)
			) {

				//Compaction stays legal once a TLAS has adopted the structure: the TLAS re-resolves rather
				// than being stuck with the old address.

				const c::U64 before = asSize(blas);

				if (compactAndRun(t, dev, blas, "compactAfterTlasAdopted")) {

					c::Test_assert(t, "compactedDespiteTlas", blas.data()->base.isCompacted);
					c::Test_assert(t, "compactedDespiteTlasShrank", asSize(blas) <= before);

					//A driver may report no saving (WARP has reported 0), in which case nothing MOVED and
					// nothing is stale: the staleness legs only mean something when the address changed.

					if(asSize(blas) >= before)
						c::Test_print(t, "Driver reported no compaction saving, skipping staleness asserts");

					else {

						//The TLAS that adopted it is marked, precisely: only structures it actually references
						// make it stale, so an unrelated TLAS is never dragged into a refill.

						c::Test_assert(t, "adoptingTlasMarkedStale",
							(tlas.data()->base.flagsExt & (c::U8) c::ETLASFlag_AddressesStale) != 0
						);

						//The submit AFTER the compaction is refused while the TLAS still holds the old address,
						// which is what keeps a moved structure from being traced. e_rr stays null; the refusal
						// is the expected result.

						{
							gfx::CommandList idle;

							if (
								c::Test_assert(t, "createIdleList", dev.createCommandList(c::KIBI, 32, 32, idle, true, e_rr)) &&
								c::Test_assert(t, "beginIdle", idle.begin(true, e_rr)) &&
								c::Test_assert(t, "endIdle", idle.end(e_rr))
							)
								c::Test_assert(t, "submitRefusedWhileTlasStale", !dev.submit({ &idle }, {}, 0, 0, nullptr));
						}

						//A list that updates the TLAS is accepted no matter when it was RECORDED, because the gate
						// asks what this submit contains. Recording one and never submitting it therefore does
						// not clear anything, and a list recorded once can be submitted again after a later
						// compaction and still satisfy the gate.

						{
							gfx::CommandList discarded;

							if (
								c::Test_assert(t, "createDiscarded", dev.createCommandList(
									c::KIBI, 32, 32, discarded, true, e_rr
								)) &&
								c::Test_assert(t, "beginDiscarded", discarded.begin(true, e_rr))
							) {
								{
									gfx::CommandScope scope = discarded.scope({}, 0, {}, e_rr);

									if (c::Test_assert(t, "scopeDiscarded", (c::Bool) scope))
										c::Test_assert(t, "updateTlasDiscarded", scope.updateTlas(tlas, e_rr));
								}

								c::Test_assert(t, "endDiscarded", discarded.end(e_rr));
							}

							//Recorded, never submitted: the TLAS is still owed an update, so an unrelated submit
							// is still refused.

							gfx::CommandList unrelated;

							if (
								c::Test_assert(t, "createUnrelated", dev.createCommandList(
									c::KIBI, 32, 32, unrelated, true, e_rr
								)) &&
								c::Test_assert(t, "beginUnrelated", unrelated.begin(true, e_rr)) &&
								c::Test_assert(t, "endUnrelated", unrelated.end(e_rr))
							)
								c::Test_assert(t, "recordingAloneDoesNotClear", !dev.submit({ &unrelated }, {}, 0, 0, nullptr));

							//And submitting the list that DOES update it is accepted, then clears the mark.

							if (discarded.valid())
								c::Test_assert(t, "submitOfUpdatingListAccepted", gfxtest::submitAndWait(t, dev, discarded));
						}

						//And the rebuild both clears that and re-points the TLAS at the structure compaction moved,
						// so submitting works again.

						gfx::CommandList rebuild;

						if (
							c::Test_assert(t, "createRebuildList", dev.createCommandList(
								c::KIBI, 32, 32, rebuild, true, e_rr
							)) &&
							c::Test_assert(t, "beginRebuild", rebuild.begin(true, e_rr))
						) {
							{
								gfx::CommandScope scope = rebuild.scope({}, 0, {}, e_rr);

								if (c::Test_assert(t, "scopeRebuild", (c::Bool) scope))
									c::Test_assert(t, "updateTlasAfterCompact", scope.updateTlas(tlas, e_rr));
							}

							if (c::Test_assert(t, "endRebuild", rebuild.end(e_rr)))
								c::Test_assert(t, "rebuildTlasOnCompacted", gfxtest::submitAndWait(t, dev, rebuild));
						}
					}
				}
			}
		}
	}

	// -- REFUSED before the build's submit has completed ---------------------

	//The compacted size is produced BY the build, so it does not exist until the build has RUN. Blocking on
	// the query would hide a caller who never submitted, turning that mistake into a hang.

	{
		gfx::Blas blas;
		gfx::CommandList list;

		if (
			c::Test_assert(t, "createBlasTiming", dev.createBlas(compactable, "Timing BLAS", blas, e_rr)) &&
			recordBuild(t, dev, list, blas, "recordBuildTiming")
		) {

			//Recorded but never submitted.

			c::Test_assert(t, "refusedBeforeSubmit", !tryRecordCompact(dev, blas));

			//Submitted but not waited on. The device tracks the highest submit known to have COMPLETED, and
			// nothing has advanced it yet, so this is a decision and not a race.

			if (c::Test_assert(t, "submitWithoutWait", dev.submit({ &list }, {}, 0, 0, e_rr))) {

				c::Test_assert(t, "refusedBeforeCompletion", !tryRecordCompact(dev, blas));

				//And the same record goes through the moment the wait makes the size readable.

				if (c::Test_assert(t, "waitAfterSubmit", dev.wait(e_rr)))
					c::Test_assert(t, "compactAfterWait", compactAndRun(t, dev, blas, "compactAfterWait"));
			}
		}
	}

	// -- A structure built without the flag is a no-op, not an error ---------

	{
		gfx::Blas blas;
		gfx::CommandList list;

		const c::BLASCreateInfo plain = c::BLASCreateInfo_unindexed(
			c::ERTASBuildFlags_None, c::EBLASFlag_None,
			c::ETextureFormatId_RGBA32f, 0, 16, positions.region()
		);

		if (
			c::Test_assert(t, "createBlasPlain", dev.createBlas(plain, "Plain BLAS", blas, e_rr)) &&
			recordBuild(t, dev, list, blas, "recordBuildPlain") &&
			gfxtest::submitAndWait(t, dev, list)
		) {
			c::Test_assert(t, "noopWithoutAllowCompaction", compactAndRun(t, dev, blas, "noopPlain"));
			c::Test_assert(t, "noopLeftItUncompacted", !blas.data()->base.isCompacted);
		}
	}

	// -- Staleness is per dependency, not device wide ------------------------

	//Two TLASes holding different structures. Compacting one TLAS's BLAS must not touch the other, since
	//a mesh arriving mid scene compacts as it loads and marking every TLAS would refill instance buffers
	//that did not change.

	{
		gfx::Blas blasA, blasB;
		gfx::Tlas tlasA, tlasB;
		gfx::CommandList listA, listB;

		const auto instanceOf = [](gfx::Blas &b) {
			return c::TLASInstance {
				.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
				.data = {
					.instanceId24_mask8 = 0xFFu << 24,
					.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_Default << 24,
					.blasCpu = b.handle()
				}
			};
		};

		if (
			c::Test_assert(t, "createBlasA", dev.createBlas(compactable, "Disjoint A", blasA, e_rr)) &&
			c::Test_assert(t, "createBlasB", dev.createBlas(compactable, "Disjoint B", blasB, e_rr)) &&
			recordBuild(t, dev, listA, blasA, "recordBuildA") &&
			gfxtest::submitAndWait(t, dev, listA) &&
			recordBuild(t, dev, listB, blasB, "recordBuildB") &&
			gfxtest::submitAndWait(t, dev, listB)
		) {

			const c::TLASInstance instA = instanceOf(blasA);
			const c::TLASInstance instB = instanceOf(blasB);

			gfx::CommandList adoptList;
			c::U64 beforeB = 0;

			if (
				c::Test_assert(t, "createTlasA", dev.createTlas(
					(c::ERTASBuildFlags) (c::ERTASBuildFlags_DefaultTLAS | c::ERTASBuildFlags_AllowUpdate),
					&instA, 1, "Disjoint TLAS A", tlasA, true, e_rr
				)) &&
				c::Test_assert(t, "createTlasB", dev.createTlas(
					(c::ERTASBuildFlags) (c::ERTASBuildFlags_DefaultTLAS | c::ERTASBuildFlags_AllowUpdate),
					&instB, 1, "Disjoint TLAS B", tlasB, true, e_rr
				)) &&

				//Built to completion first, for the same reason as the publish phase above

				c::Test_assert(t, "createAdoptListB", dev.createCommandList(c::KIBI, 32, 32, adoptList, true, e_rr)) &&
				c::Test_assert(t, "beginAdoptListB", adoptList.begin(true, e_rr)) &&
				c::Test_assert(t, "endAdoptListB", adoptList.end(e_rr)) &&
				gfxtest::submitAndWait(t, dev, adoptList) &&
				((beforeB = asSize(blasB)), compactAndRun(t, dev, blasB, "compactB"))
			) {

				//Meaningful only when the structure actually moved; see the publish phase above.

				if(asSize(blasB) >= beforeB)
					c::Test_print(t, "Driver reported no compaction saving, skipping disjoint staleness asserts");

				else {

					const c::U8 stale = (c::U8) c::ETLASFlag_AddressesStale;

					c::Test_assert(t, "disjointTlasBMarked",  (tlasB.data()->base.flagsExt & stale) != 0);
					c::Test_assert(t, "disjointTlasAUntouched", !(tlasA.data()->base.flagsExt & stale));
				}

				//And the TLAS that was not marked does not need rebuilding to keep submitting.

				gfx::CommandList rebuildB;

				if (
					c::Test_assert(t, "createRebuildB", dev.createCommandList(c::KIBI, 32, 32, rebuildB, true, e_rr)) &&
					c::Test_assert(t, "beginRebuildB", rebuildB.begin(true, e_rr))
				) {
					{
						gfx::CommandScope scope = rebuildB.scope({}, 0, {}, e_rr);

						if (c::Test_assert(t, "scopeRebuildB", (c::Bool) scope))
							c::Test_assert(t, "updateTlasB", scope.updateTlas(tlasB, e_rr));
					}

					if (c::Test_assert(t, "endRebuildB", rebuildB.end(e_rr)))
						c::Test_assert(t, "submitAfterRebuildB", gfxtest::submitAndWait(t, dev, rebuildB));
				}
			}
		}
	}

	// -- The storage grows past its first pool, and slots come back ----------

	//Every structure here is built before any of them is compacted, so all of them hold a slot at once and
	// the storage has to have grown for this to pass. A cap would not fail loudly: past it a structure just
	// never compacts and the memory does not come back.

	{
		gfx::Blas blases[CHUNK_CROSSING_COUNT];
		gfx::CommandList list;

		//One triangle each: this case is about slot bookkeeping, not about geometry.

		const c::F32 triangle[12] = {
			0, 0, 0, 1,
			1, 0, 0, 1,
			0, 1, 0, 1
		};

		c::Buffer triData = c::Buffer_createRefConst(triangle, sizeof(triangle));
		gfx::DeviceBuffer tri;

		c::Bool ok = c::Test_assert(t, "createTriPositions", dev.createBufferData(
			c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
			"Compaction tri", &triData, tri, nullptr, e_rr
		));

		const c::BLASCreateInfo small = c::BLASCreateInfo_unindexed(
			c::ERTASBuildFlags_AllowCompaction, c::EBLASFlag_None,
			c::ETextureFormatId_RGBA32f, 0, 16, tri.region()
		);

		ok = ok &&
			c::Test_assert(t, "createChunkList", dev.createCommandList(c::MIBI, 1024, 1024, list, true, e_rr)) &&
			c::Test_assert(t, "beginChunkList", list.begin(true, e_rr));

		if (ok) {

			gfx::CommandScope scope = list.scope({}, 0, {}, e_rr);

			ok = c::Test_assert(t, "scopeChunk", (c::Bool) scope);

			for (c::U32 i = 0; ok && i < CHUNK_CROSSING_COUNT; ++i)
				ok =
					dev.createBlas(small, "Chunk BLAS", blases[i], e_rr) &&
					scope.updateBlas(blases[i], e_rr);

			c::Test_assert(t, "buildAllChunkBlases", ok);
		}

		if (ok && c::Test_assert(t, "endChunkList", list.end(e_rr)) && gfxtest::submitAndWait(t, dev, list)) {

			//Every compaction records into ONE list and runs in ONE submit, which is the whole point of the
			// copy being a recorded op: no part of this stalls the device.

			gfx::CommandList compactList;

			c::Bool recorded =
				c::Test_assert(t, "createCompactList", dev.createCommandList(c::MIBI, 1024, 1024, compactList, true, e_rr)) &&
				c::Test_assert(t, "beginCompactList", compactList.begin(true, e_rr));

			if (recorded) {

				gfx::CommandScope scope = compactList.scope({}, 0, {}, e_rr);

				recorded = c::Test_assert(t, "scopeCompact", (c::Bool) scope);

				for (c::U32 i = 0; recorded && i < CHUNK_CROSSING_COUNT; ++i)
					recorded = scope.compactBlas(blases[i], e_rr);

				c::Test_assert(t, "recordedEveryChunkCompaction", recorded);
			}

			if (
				recorded && c::Test_assert(t, "endCompactList", compactList.end(e_rr)) &&
				gfxtest::submitAndWait(t, dev, compactList)
			) {

				//The flag, not a count of successful calls: a compaction that silently did nothing would
				// still report success.

				c::U32 marked = 0;

				for (c::U32 i = 0; i < CHUNK_CROSSING_COUNT; ++i)
					if (blases[i].data()->base.isCompacted)
						++marked;

				c::Test_assert(t, "chunkCrossingAllMarked", marked == CHUNK_CROSSING_COUNT);
			}
		}
	}
}
