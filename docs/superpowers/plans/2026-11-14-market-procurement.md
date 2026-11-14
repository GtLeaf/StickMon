# Market Procurement Implementation Plan

> **For agentic workers:** Execute approved requirements directly. Independent stock rules, persistence and shop UI use dispatching-parallel-agents with exact contracts; root integrates App/strings/tutor/tests and one independent final review. No commits/push/flash requested.

**Goal:** Add Kecleon home/advice, fixed-total next-period supply preference, randomized concrete nutrients/berries, larger detail quantities/stocks, Gengar choose prompt.
**Architecture:** Retain eight shelf identities and private period draws. Advice changes quantities only; choice consumed on next actual market opening. Append an explicit aligned state10/schema9 extension for pending advice, current stock plan and second box draw, freezing all2524 v9bytes.
**Tech Stack:** C++17, existing MarketListScroll/panels, SaveCodec, native/App hosts.
**Spec:** Current user-approved plan plus fixed16 correction; newest dialogue exact 知道啦！下次多备些！. Existing pending stall-scroll changes preserved. No battlepotionfix scope.

## Global Constraints
-8positions,total16every newperiod; prices/item pools/ordinaryshopgates unchanged. Nutrient:uniform1of6 stockdefault2;EVberry1of6 stockdefault3, concreteitemshownbeforebuy, noselector, no choice override. Freeze draw throughentry/restart/teamchanges and savefailures.
- Supply Advice enum0NONE,1EXPLORE,2GROWTH,3RARE. Planenum0DEFAULT,1EXPLORE,2GROWTH,3RARE_A,4RARE_B,5RARE_C. Capacities in slotorderdiscountA/B,candy,nutrient,berry,special,rare,box:
  default[3,3,1,2,3,2,1,1];explore[4,4,1,1,2,2,1,1];growth[2,2,1,3,4,2,1,1];rareA[2,3,2,2,3,2,1,1];rareB[3,2,1,2,3,2,1,2];rareC[2,2,2,2,3,2,1,2]. Rarethreeplans equalchance, at least candy orboxincreases; allsum16. Categoryaffectsstockonly, notselectionpools orprices.
- Advice saved immediately and maychange duringcurrentopenperiod; current shelf untouched. Consume pending once at nextopening then resetNONE; noadvice nexttimeDEFAULT. Apply via MarketShop::advise(state,Advice,period)->MarketShop::Result, validatesopen/currentperiod/1..3.
- StateGameState.marketSupply appended as alignas(4) struct{uint8_t pendingAdvice=0,stockPlan=0,boxRoll2=0,reserved=0;};offset2524,size4,GameStatesize2528/version10. Codec9appends3logicalbytes aftergift beforeview. Schemas1..8 andfrozenraw9(2524prefix)/raw8 retained, noreplenishingoldperiodstock. MetadataCapsvalidation usescurrentstockplan andremaining sum<=16.
- marketSeason.boxRoll retainsfirstdraw, marketSupply.boxRoll2 required1..100 onlywhenplaninitialBOX2. Beforefirstpurchase usefirstroll, afterremainingBOX1 use second. Rewardsindependentlygenerated/frozen, duplicatesallowed. Wholecapacity/precheck/atomicrollback retained. Up to2directcandies+2boxbonus=4theoretical.
- UI Request remains kindNONE/BUY plusADVISE; fields slot,period,advice(enum), removechoice. API committed(bool,item,amount), adviceCommitted(bool), rejected(Result). MENUaftergreet:购买道具/进货建议;SHELFoutside->MENU,MENUoutsidecloses;ADVICE3rows+Back;selectsavethenADVICE_DONE showsdialogueexact,button返回首页/outside->MENU. Currentpendingadvice highlighted. Detail×1/库存both32px;headericon48name+qty, descriptionmax2lines,stockdirectlybelow;bottomBack/Buy retained. Gengarthreechoosepagesbottomcenter16px请选择精灵.
- Existing inertia, panel positions/corners/framecadence unchanged. Appnewadvicesnapshotatomic likepurchases; oldstockbyte-for-byte retained untilnewopening. Fontsappendonlyifneeded; noexternal-map regeneration.

## Interfaces / ownership
- Rulesworker: NEW src/game/MarketSupply.h (pure enums, capacities(Plan)->constu8*,validPlan/validAdvice/initialBoxStock), MarketShopService.h andfocusedrulehosts. PublishMarketSupply.h early forSaveworker.
- Persistenceworker: SaveCodec.h/cpp,SaveManager.cpp/h,savefabricator+codec/migrationhosts. RootownsGameState.h. Use pureMarketSupplycaps ratherthanduplicate.
- ShopUIworker: MarketShopUi.h,market_selector_host.cpp andnewfocusedUIhosts; rootownsUiStrings+App+seasonApphost. PreserveMarketListScroll integration,returnscrollposition. Relyonagreedservice/RequestAPIs.
- Root: GameState,handoffcontracts,UiStrings,Gengarprompt,Appintegration,fullAppjourney,newnativewrapper,fonts/specs andfinalreview.

## Review Focus
Advice rollback/partialsave/restart, frozen16stock/currentperiodpreservation; randomconcretechoicescannotoverride; secondboxordinal/reset/fullstackfailure cannotreroll; codec8/raw9oldquota/defaultmigration; gesturecapture/newpageoutside/oldselectorremoved/largefontbounds.

## Tasks
- [x]1 RecordREDABI/behavior thenappendState10; keepfeaturebranchcurrentuncommittedwork.
- [x]2 RulesRED→GREEN: allplanssum16, ordinaryunchanged,nextperiodonly,pendingconsume,randomcoverage/frozen draw, twofixedboxrolls/refusals, nochoiceoverride.
- [x]3 PersistenceRED→GREEN: schema9roundtrip,schemas1..8/raw9+8migration/defaults,newcapssanitization,partial-writecontract unchanged.
- [x]4 ShopUIRED→GREEN:home/advice/savefeedback,concretedetailnoselector,largeqty/stock,selectedpreference,alloutside/backroutes,soldout/scroll.
- [x]5 Rootintegration:exactstrings/tutorprompt,atomicAppadvice,full3periodpreferencejourney+reload/failures,two-boxwhole-rewardchecks,pages/animationexport.
- [x]6 Validate:rules/UI/save/nativeApp/tutor/care/architecture/resourceglyphcoverage,independentreview,docs+ledger,reportfirmware/devicegaps.

## Completion evidence
All tasks completed. Evidence: `.stick-local/artifacts/market-procurement-20261114/verification.md`; 20-page contact.png and flow.gif. Native App/rules/UI/save checks and independent review passed; no firmware build/device/commit/push/merge.
