// BGED Sort Fix
//
// Worn meshes can bring their own animation graph through BSBehaviorGraphExtraData ("BGED"):
// SOS/TNG genitals use it for bends, erections and the flaccid pose. The game finds that extra
// data with NiObjectNET::GetExtraData, a binary search over the node's extra data, which must
// therefore stay sorted by the names' (64-bit) string-pool addresses.
//
// RaceMenu's skee64 (0.4.20) adds SHAPEDATA to body-morphed shapes and then re-sorts the whole
// array comparing only the low 32 bits of those addresses. With the strings more than 4 GB apart
// (EngineFixes' memory manager), that order is wrong for the game. A single-shape genital mesh has
// its BGED moved onto its shape when attached, so the search can miss it:
//   - fresh attach (load, cell change): no graph at all;
//   - disable/enable, re-equip: graph bound to the mesh instead of the skeleton, every bone missing.
// Either way bend/erection/flaccid events do nothing. Which one happens is fixed per game launch.
//
// Fix: right before the game sets up a biped part's graph, put the part's extra data back in the
// order the game expects.

namespace
{
	void InitializeLog()
	{
		auto path = logger::log_directory();
		if (!path) {
			SKSE::stl::report_and_fail("Failed to find the SKSE log directory");
		}
		*path /= "BGEDSortFix.log";
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
		auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");
	}

	// AE address library IDs (checked on 1.6.1170).
	constexpr REL::ID kSetupGraph{ 15718 };     // bool BipedAnim::SetupGraph(slot, ref)
	constexpr REL::ID kExtraDataLock{ 410492 };  // the lock NiObjectNET's extra data functions take

	// The calls to SetupGraph: right after a part is attached, the "every slot without a graph"
	// pass (3D (re)load), and the weapon/armor attach path.
	struct CallSite
	{
		REL::ID        caller;
		std::ptrdiff_t offset;
		const char*    name;
	};
	constexpr CallSite kCallSites[] = {
		{ REL::ID(15678), 0xE9F, "part attach" },
		{ REL::ID(15680), 0x51, "all slots" },
		{ REL::ID(15683), 0x321, "item attach" },
	};

	using SetupGraph_t = bool(RE::BipedAnim*, std::int32_t, RE::TESObjectREFR*);
	std::uintptr_t originals[std::size(kCallSites)]{};

	bool NameLess(const RE::NiExtraData* a_lhs, const RE::NiExtraData* a_rhs)
	{
		if (!a_lhs || !a_rhs) {
			return a_lhs && !a_rhs;  // nulls last
		}
		return reinterpret_cast<std::uintptr_t>(a_lhs->GetName().c_str()) < reinterpret_cast<std::uintptr_t>(a_rhs->GetName().c_str());
	}

	// True if the order had to be fixed.
	bool SortExtraData(RE::NiAVObject* a_node)
	{
		if (!a_node || !a_node->extra || a_node->extraDataSize < 2) {
			return false;
		}
		const auto first = a_node->extra;
		const auto last = first + a_node->extraDataSize;
		REL::Relocation<RE::BSReadWriteLock*> lock{ kExtraDataLock };
		RE::BSWriteLockGuard                  guard{ *lock };
		if (std::is_sorted(first, last, NameLess)) {
			return false;
		}
		std::sort(first, last, NameLess);
		return true;
	}

	template <std::size_t I>
	bool SetupGraph(RE::BipedAnim* a_biped, std::int32_t a_slot, RE::TESObjectREFR* a_ref)
	{
		if (a_biped && a_slot >= 0 && a_slot < RE::BIPED_OBJECTS::kTotal) {
			const auto part = a_biped->objects[a_slot].partClone.get();
			if (SortExtraData(part)) {
				logger::info("Re-sorted the extra data of slot {} ({}) on {:08X} {} ({})", a_slot + 30,
					part->name.c_str() ? part->name.c_str() : "", a_ref ? a_ref->GetFormID() : 0,
					a_ref && a_ref->GetName() ? a_ref->GetName() : "", kCallSites[I].name);
			}
		}
		return reinterpret_cast<SetupGraph_t*>(originals[I])(a_biped, a_slot, a_ref);
	}

	template <std::size_t... Is>
	std::size_t InstallHooks(std::index_sequence<Is...>)
	{
		std::size_t installed = 0;
		(
			[&] {
				const auto site = kCallSites[Is].caller.address() + kCallSites[Is].offset;
				const auto code = reinterpret_cast<const std::uint8_t*>(site);
				if (code[0] != 0xE8) {
					logger::warn("{}: unexpected code at the call site, not patched", kCallSites[Is].name);
					return;
				}
				const auto target = site + 5 + *reinterpret_cast<const std::int32_t*>(site + 1);
				originals[Is] = SKSE::GetTrampoline().write_call<5>(site, SetupGraph<Is>);
				logger::info("{}: patched{}", kCallSites[Is].name, target == kSetupGraph.address() ? "" : " (after another plugin's hook)");
				++installed;
			}(),
			...);
		return installed;
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();
	SKSE::Init(a_skse);

	const auto plugin = SKSE::PluginDeclaration::GetSingleton();
	const auto version = REL::Module::get().version();
	logger::info("{} {} (game {})", plugin->GetName(), plugin->GetVersion().string("."), version.string("."));

	if (!REL::Module::IsAE()) {
		logger::warn("Only Skyrim AE (1.6.x) is supported; nothing patched");
		return true;
	}
	SKSE::AllocTrampoline(14 * std::size(kCallSites));
	const auto installed = InstallHooks(std::make_index_sequence<std::size(kCallSites)>{});
	logger::info("{} of {} call sites patched", installed, std::size(kCallSites));
	return true;
}
