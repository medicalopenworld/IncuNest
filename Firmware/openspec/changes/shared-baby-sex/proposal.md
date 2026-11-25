## Why

Boards affected: **motherBoard** (stores and publishes the sex) and **Display_HMI** (asks for it). `shared/` is untouched.

IncuTwin, the family-facing twin of the incubator, gives every baby a name and a drawing (boy or girl). It decides both in ThingsBoard, once per stay, so that every IncuTwin paired to the same incubator shows the same baby. Today IncuNest never says the baby's sex, so the cloud always picks one at random. With the sex the nurse already knows at admission, the twin shows the right one.

The data is optional on purpose: "not recorded" is a valid answer, the registration and the therapy go on exactly as before, and the cloud keeps choosing when it is 0.

## What Changes

- **New protocol message `HMI,PROFILE_SEX,<seq>,<sex>`** (ISO/IEC 5218: 0 not recorded, 1 male, 2 female). Sent by the HMI right after the `CTRL,PROFILE_ACK` of a `PROFILE_NEW`, before the weight. No reply. **Not breaking in either direction**: `PROFILE_NEW` keeps its 4 fields; an old board discards the unknown line (log only); an old HMI never sends it (sex stays 0).
- **`BabyProfile.sex`** in the padding byte after `gestWeeks`. Active slots are raw NVS blobs and a size change wipes all three on upgrade; using the padding keeps `sizeof(BabyProfile) == 64`, frozen with `static_assert`. Old blobs read 0 = not recorded.
- **Client attribute `baby_sex`**, always present (also 0, also in the empty-incubator payload): an omitted key keeps its previous value in ThingsBoard and the next baby would inherit it. Also in the discharge event row.
- **Not in the 60-byte audit record**: growing it resets the clinical history. Archived babies read back as 0.
- **HMI: a "Baby's sex" step** (BOY / GIRL / NOT RECORDED, one tap advances) between name and gestational weeks, both in the activation wizard and in the Babies-screen registration. Nurse training lessons renumbered (1/4..4/4).

## Capabilities

### Modified Capabilities
- `baby-profile-protocol`: new `PROFILE_SEX` message; `baby_sex` client attribute.
