# SDGOODS item details implementation plan

> Use superpowers:executing-plans inline; continue user-authorized migration.

**Goal:** Native round bag and ordinary-shop text uses approved automatic wrap.
**Architecture:** Round-only ItemDetailView owns typography/description geometry;
bag/shop keep state, eligibility, transactions and selection. TextBlock owns wrap.
**Tech Stack:** Existing C++17 Canvas565/TextBlock/native engine.
**Spec:** specs/sdgoods-item-details.md.

## Global Constraints
Preserve5earlier uncommitted batches and current feature branch; no new rules,
schema/resources/palette; no V1/OSTB builds, flash, commit, push or merge.

## Review Focus
- Long names/descriptions and punctuation cannot overlap metadata/actions.
- Circular viewport, dirty clips and swapped byte-order retain all pixels.
- Read-only, battle feeding, target selection and home-only restrictions unchanged.
- Touch/CANCEL/drag ownership and physical buttons keep correct item identity.
- Ordinary prices/balances, buy/sell effects and return context remain authoritative.

## Task1: Native details and proof
Files: new round ItemDetailView.h; round BagUi.cpp/ShopUi.cpp/.h; new native
item-detail host/runner and reusable native test builder host argument.
- [x] Capture old details, assert paragraph body matches native16px wrapping/4px gap;
      run focused test RED due old/manual line breaks and narrow body geometry.
- [x] Add shared round helper, migrate names/metadata/descriptions/button labels
      and safe geometry; keep lists/header/tabs/coins and shared rules.
- [x] All items×home/battle/buy/sell fit without truncation; actual text region,
      dirty/swap/purity, input and real buy/sell/use journeys GREEN.
- [x] Staged ASan/UBSan and representative before/after; shared text/save/
      architecture regressions, SDGOODS+V2 builds; one fresh final review.
- [x] Retain verified source uncommitted, record evidence and limitations.
