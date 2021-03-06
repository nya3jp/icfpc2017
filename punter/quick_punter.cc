#include "punter/quick_punter.h"

#include "base/base64.h"
#include "base/memory/ptr_util.h"

namespace punter {

QuickPunter::QuickPunter() = default;
QuickPunter::~QuickPunter() = default;

void QuickPunter::SetUp(const common::SetUpData& args) {
  proto_.set_punter_id(args.punter_id);
  framework::GameMapProto* game_map_proto = proto_.mutable_game_map();
  for (size_t i = 0; i < args.game_map.sites.size(); ++i) {
    const auto& site = args.game_map.sites[i];
    framework::SiteProto* site_proto = game_map_proto->add_sites();
    site_proto->set_id(site.id);
  }
  for (auto& r : args.game_map.rivers) {
    framework::RiverProto* river_proto = game_map_proto->add_rivers();
    river_proto->set_source(r.source);
    river_proto->set_target(r.target);
    river_proto->set_punter(-1);
  }
  for (auto& m : args.game_map.mines) {
    game_map_proto->add_mines()->set_site(m);
    QuickPunterNodeColor* color_proto = proto_.add_node_color();
    color_proto->set_site(m);
    color_proto->set_mine(m);
    node_color_.insert(std::make_pair(m, m));
  }
}

void QuickPunter::ClaimRiver(int punter_id, int source, int target) {
  framework::GameMapProto* game_map_proto = proto_.mutable_game_map();
  for (int i = 0; i < game_map_proto->rivers_size(); i++) {
    framework::RiverProto* r = game_map_proto->mutable_rivers(i);
    if ((r->source() == source && r->target() == target) ||
        (r->source() == target && r->target() == source)) {
      r->set_punter(punter_id);
      if (punter_id != proto_.punter_id())
        continue;
      auto src_color = node_color_.find(source);
      auto tgt_color = node_color_.find(target);
      if (src_color != node_color_.end() &&
          tgt_color == node_color_.end()) {
        node_color_.insert(std::make_pair(target, src_color->second));
        QuickPunterNodeColor* color_proto = proto_.add_node_color();
        color_proto->set_site(target);
        color_proto->set_mine(src_color->second);
      } else if (src_color == node_color_.end() &&
                 tgt_color != node_color_.end()) {
        node_color_.insert(std::make_pair(source, tgt_color->second));
        QuickPunterNodeColor* color_proto = proto_.add_node_color();
        color_proto->set_site(source);
        color_proto->set_mine(tgt_color->second);
      }
    }
  }
}

framework::GameMove QuickPunter::Run(const std::vector<framework::GameMove>& moves) {
  // Mark the claimed rivers
  for (auto& move : moves) {
    switch (move.type) {
    case framework::GameMove::Type::PASS:
      break;
    case framework::GameMove::Type::CLAIM:
      ClaimRiver(move.punter_id, move.source, move.target);
      break;
    case framework::GameMove::Type::SPLURGE:
      for (size_t i = 1; i < move.route.size(); i++) {
        ClaimRiver(move.punter_id, move.route[i-1], move.route[i]);
      }
      break;
    case framework::GameMove::Type::OPTION:
      // Option moves by others are ignored.
      if (move.punter_id == proto_.punter_id())
        ClaimRiver(move.punter_id, move.source, move.target);
      break;
    }
  }

  // Select a move
  for (auto& r : proto_.game_map().rivers()) {
    if (r.punter() != -1)
      continue;
    auto src_color = node_color_.find(r.source());
    auto tgt_color = node_color_.find(r.target());

    if ((src_color == node_color_.end() && tgt_color != node_color_.end()) ||
        (src_color != node_color_.end() && tgt_color == node_color_.end()) ||
        (src_color != node_color_.end() && tgt_color != node_color_.end() &&
         src_color->second != tgt_color->second)) {
      return {framework::GameMove::Type::CLAIM, proto_.punter_id(),
          r.source(), r.target()};
    }
  }

  // We cannot gain more points, but claim a edge to disturb other punters.
  for (auto& r : proto_.game_map().rivers()) {
    if (r.punter() == -1) {
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

  for (auto& node_color : proto_.node_color()) {
    node_color_.insert(std::make_pair(node_color.site(), node_color.mine()));
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
