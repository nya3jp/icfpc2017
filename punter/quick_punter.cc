#include "punter/quick_punter.h"

#include "base/base64.h"
#include "base/memory/ptr_util.h"

namespace punter {

QuickPunter::QuickPunter() = default;
QuickPunter::~QuickPunter() = default;

void QuickPunter::SetUp(int punter_id, int num_punters, const framework::GameMap& game_map) {
  proto_.set_punter_id(punter_id);
  framework::GameMapProto* game_map_proto = proto_.mutable_game_map();
  for (size_t i = 0; i < game_map.sites.size(); ++i) {
    const auto& site = game_map.sites[i];
    framework::SiteProto* site_proto = game_map_proto->add_sites();
    site_proto->set_id(site.id);
  }
  for (auto& r : game_map.rivers) {
    framework::RiverProto* river_proto = game_map_proto->add_rivers();
    river_proto->set_source(r.source);
    river_proto->set_target(r.target);
    river_proto->set_punter(-1);
  }
  for (auto& m : game_map.mines) {
    game_map_proto->add_mines()->set_site(m);
    proto_.add_sites_connected_to_mine(m);
    sites_connected_to_mine_.insert(m);
  }
}

framework::GameMove QuickPunter::Run(const std::vector<framework::GameMove>& moves) {
  // Mark the claimed rivers
  framework::GameMapProto* game_map_proto = proto_.mutable_game_map();
  for (auto& move : moves) {
    if (move.type != framework::GameMove::Type::CLAIM)
      continue;
    for (int i = 0; i < game_map_proto->rivers_size(); i++) {
      framework::RiverProto* r = game_map_proto->mutable_rivers(i);
      if ((r->source() == move.source && r->target() == move.target) ||
          (r->source() == move.target && r->target() == move.source)) {
        DCHECK(r->punter() == -1);
        r->set_punter(move.punter_id);
        if (move.punter_id == proto_.punter_id()) {
          if (sites_connected_to_mine_.insert(move.source).second)
            proto_.add_sites_connected_to_mine(move.source);
          if (sites_connected_to_mine_.insert(move.target).second)
            proto_.add_sites_connected_to_mine(move.target);
        }
      }
    }
  }

  // Select a move
  for (auto& r : game_map_proto->rivers()) {
    if (r.punter() != -1)
      continue;
    if (sites_connected_to_mine_.count(r.source()) ||
        sites_connected_to_mine_.count(r.target())) {
      return {framework::GameMove::Type::CLAIM, proto_.punter_id(),
          r.source(), r.target()};
    }
  }
  return {framework::GameMove::Type::PASS, proto_.punter_id()};
}

void QuickPunter::SetState(std::unique_ptr<base::Value> state_in) {
  auto state = base::DictionaryValue::From(std::move(state_in));

  std::string b64_proto;
  CHECK(state->GetString("proto", &b64_proto));
  std::string serialized;
  CHECK(base::Base64Decode(b64_proto, &serialized));
  CHECK(proto_.ParseFromString(serialized));

  for (int site : proto_.sites_connected_to_mine()) {
    sites_connected_to_mine_.insert(site);
  }
}

std::unique_ptr<base::Value> QuickPunter::GetState() {
  auto value = base::MakeUnique<base::DictionaryValue>();

  const std::string binary = proto_.SerializeAsString();
  std::string b64;
  base::Base64Encode(binary, &b64);
  value->SetString("proto", b64);

  return std::move(value);
}

} // namespace punter
