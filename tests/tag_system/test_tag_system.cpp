#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>

#include "weatherpaper/tag_system/tag_system.hpp"

using namespace weatherpaper::tag_system;
using weatherpaper::scaling_and_fit::FitMode;

namespace {
AssetRecord make(std::string id, std::vector<std::string> tags,
                  AssetType type = AssetType::Image,
                  FitMode fit = FitMode::Fill) {
    AssetRecord r;
    r.id = id;
    r.file_path = id + ".jpg";
    r.type = type;
    r.fit_mode = fit;
    for (auto& t : tags) r.tags.insert(t);
    return r;
}
} // namespace

TEST_CASE("upsert/get/remove basic CRUD") {
    TagIndex idx;
    idx.upsert(make("a1", {"rain", "night"}));
    CHECK(idx.list_all().size() == 1);

    auto got = idx.get("a1");
    REQUIRE(got.has_value());
    CHECK(got->has_tag("rain"));
    CHECK_FALSE(got->has_tag("snow"));

    // upsert with same id replaces, doesn't duplicate
    idx.upsert(make("a1", {"snow", "night"}));
    CHECK(idx.list_all().size() == 1);
    CHECK(idx.get("a1")->has_tag("snow"));
    CHECK_FALSE(idx.get("a1")->has_tag("rain"));

    CHECK(idx.remove("a1"));
    CHECK(idx.list_all().empty());
    CHECK_FALSE(idx.remove("a1")); // already gone
}

TEST_CASE("set_tags / add_tag / remove_tag / set_fit_mode mutate the right record") {
    TagIndex idx;
    idx.upsert(make("a1", {"sunny"}));
    idx.add_tag("a1", "day");
    CHECK(idx.get("a1")->tags.size() == 2);

    idx.remove_tag("a1", "sunny");
    CHECK(idx.get("a1")->tags.size() == 1);
    CHECK(idx.get("a1")->has_tag("day"));

    idx.set_fit_mode("a1", FitMode::Center);
    CHECK(idx.get("a1")->fit_mode == FitMode::Center);

    idx.set_tags("a1", {"storm", "evening", "custom-mood"});
    CHECK(idx.get("a1")->tags.size() == 3);
    CHECK(idx.get("a1")->has_tag("custom-mood")); // free-text custom tags supported
}

TEST_CASE("find_matching_all requires ALL tags present (AND semantics), many-to-many") {
    TagIndex idx;
    idx.upsert(make("rain_night_1", {"rain", "night"}));
    idx.upsert(make("rain_night_2", {"rain", "night", "storm"})); // extra tag still matches
    idx.upsert(make("rain_day", {"rain", "day"}));
    idx.upsert(make("sunny_night", {"sunny", "night"}));

    auto matches = idx.find_matching_all({"rain", "night"});
    CHECK(matches.size() == 2);
    for (const auto& m : matches) {
        CHECK(m.has_tag("rain"));
        CHECK(m.has_tag("night"));
    }
}

TEST_CASE("resolve_selection: Sequential cycles deterministically through matches") {
    TagIndex idx;
    idx.upsert(make("b", {"rain", "night"}));
    idx.upsert(make("a", {"rain", "night"}));
    idx.upsert(make("c", {"rain", "night"}));
    // find_matching_all sorts by id -> expected order a, b, c

    std::vector<std::string> seen;
    for (int i = 0; i < 6; ++i) {
        auto pick = idx.resolve_selection("rain+night", {"rain", "night"},
                                           SelectionPolicy::Sequential);
        REQUIRE(pick.has_value());
        seen.push_back(pick->id);
    }
    std::vector<std::string> expected = {"a", "b", "c", "a", "b", "c"};
    CHECK(seen == expected);
}

TEST_CASE("resolve_selection: Random always returns a valid match") {
    TagIndex idx;
    idx.upsert(make("x", {"fog", "morning"}));
    idx.upsert(make("y", {"fog", "morning"}));

    for (int i = 0; i < 20; ++i) {
        auto pick = idx.resolve_selection("fog+morning", {"fog", "morning"},
                                           SelectionPolicy::Random);
        REQUIRE(pick.has_value());
        CHECK((pick->id == "x" || pick->id == "y"));
    }
}

TEST_CASE("resolve_selection: Pinned returns the pinned asset, falls back gracefully") {
    TagIndex idx;
    idx.upsert(make("p1", {"clear", "day"}));
    idx.upsert(make("p2", {"clear", "day"}));

    idx.pin("clear+day", "p2");
    auto pick = idx.resolve_selection("clear+day", {"clear", "day"}, SelectionPolicy::Pinned);
    REQUIRE(pick.has_value());
    CHECK(pick->id == "p2");

    // Pin an id that no longer matches -> falls back to Sequential rather
    // than returning nullopt.
    idx.pin("clear+day", "does-not-exist");
    auto fallback = idx.resolve_selection("clear+day", {"clear", "day"}, SelectionPolicy::Pinned);
    REQUIRE(fallback.has_value());
    CHECK((fallback->id == "p1" || fallback->id == "p2"));

    idx.unpin("clear+day");
}

TEST_CASE("resolve_selection returns nullopt when there are no matches at all") {
    TagIndex idx;
    idx.upsert(make("only", {"snow", "night"}));
    auto pick = idx.resolve_selection("storm+day", {"storm", "day"}, SelectionPolicy::Sequential);
    CHECK_FALSE(pick.has_value());
}

TEST_CASE("save_to_file / load_from_file JSON round-trip preserves all fields") {
    const std::string path = (std::filesystem::temp_directory_path() / "wp_tag_index_test.json").string();
    std::remove(path.c_str());

    TagIndex idx;
    AssetRecord r = make("vid1", {"storm", "night"}, AssetType::Video, FitMode::Fit);
    r.source_pack_id = "bundled-default";
    idx.upsert(r);
    idx.upsert(make("img1", {"sunny", "day", "my custom tag"}, AssetType::Image, FitMode::Tile));

    REQUIRE(idx.save_to_file(path));

    bool ok = false;
    TagIndex loaded = TagIndex::load_from_file(path, &ok);
    CHECK(ok);
    CHECK(loaded.list_all().size() == 2);

    auto v = loaded.get("vid1");
    REQUIRE(v.has_value());
    CHECK(v->type == AssetType::Video);
    CHECK(v->fit_mode == FitMode::Fit);
    CHECK(v->source_pack_id == "bundled-default");
    CHECK(v->has_tag("storm"));
    CHECK(v->has_tag("night"));

    auto i2 = loaded.get("img1");
    REQUIRE(i2.has_value());
    CHECK(i2->fit_mode == FitMode::Tile);
    CHECK(i2->has_tag("my custom tag"));

    std::remove(path.c_str());
}

TEST_CASE("load_from_file on a missing file returns an empty-but-valid index, not an error") {
    bool ok = false;
    TagIndex idx = TagIndex::load_from_file("/nonexistent/path/does_not_exist.json", &ok);
    CHECK(ok); // "not found" is treated as first-run, not failure
    CHECK(idx.list_all().empty());
}

TEST_CASE("load_from_file on malformed JSON degrades to an empty index rather than throwing") {
    const std::string path = (std::filesystem::temp_directory_path() / "wp_tag_index_bad.json").string();
    {
        std::ofstream out(path);
        out << "{ this is not valid json ";
    }
    bool ok = true;
    CHECK_NOTHROW(TagIndex::load_from_file(path, &ok));
    TagIndex idx = TagIndex::load_from_file(path, &ok);
    CHECK_FALSE(ok);
    CHECK(idx.list_all().empty());
    std::remove(path.c_str());
}

TEST_CASE("standard_tags vocabulary matches spec's weather conditions and time buckets") {
    auto weather = standard_tags::all_weather_tags();
    CHECK(weather.size() == 7);
    auto time = standard_tags::all_time_tags();
    CHECK(time.size() == 4);
}
