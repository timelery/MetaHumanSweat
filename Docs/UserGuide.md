# MetaHuman Sweat: user guide

## How it works

A character with a *MetaHuman Sweat* component carries two levels, each from 0 to 1.

| Level | Rises with | Falls | Draws |
|---|---|---|---|
| **Sweat** | Effort | all the time | a damp sheen in patches, beads that grow and multiply, drops that run downhill |
| **Wetness** | Rain Exposure, Immersion | all the time | a thin film and fine droplets everywhere |

What you see as the sweat level rises:

| Sweat level | On the skin |
|---|---|
| 0 | Matte skin, clearly drier than the stock MetaHuman look |
| first trace | A damp sheen, strongest on the forehead, temples, neck, sternum, shoulders and upper back |
| rising | Small beads appear, grow and multiply |
| about 0.2 | Drops start at fixed points and slide downhill, each leaving a thin bright trail |
| about 0.65 | Drops are at full strength |

The component does not change your materials. At Begin Play it looks for the *sweat version* of each skin material on the actor's skeletal mesh components, puts a dynamic instance of it in the slot, and writes the levels into it. When the component ends or is switched off it puts back what was there.

The sweat versions are made once per character by the editor tools (next section).

## Preparing a character

**Tools > MetaHuman Sweat** has three commands.

| Command | Use it when | What it does |
|---|---|---|
| **Add Sweat to Selected Actors** | the character is in a level | Makes the sweat materials for every skin slot of the selected actors and adds a *MetaHuman Sweat* component to each (on that level actor only). |
| **Add Sweat to Selected Skeletal Meshes** | you work from the Content Browser | Makes the sweat materials for the selected body and face meshes. Add the component in the character's Blueprint yourself. |
| **Update Sweat Materials** | you updated the plugin or edited its shader files | Rebuilds the sweat block in every sweat master material and recompiles it. |

Each command shows a report: what it created, what it skipped and why. Running a command again is safe; it creates nothing twice.

What gets created, all under `/Game/MetaHumanSweat` (the folder is a project setting):

| Asset | What it is |
|---|---|
| `Textures/T_MHSweat_FaceMask`, `T_MHSweat_BodyMask` | Where sweat forms. Imported from the plugin's `Resources/Masks`. |
| `Materials/M_Sweat_<Master>_<Hash>` | A copy of your skin master material with the sweat block added in front of its output. One per skin master. |
| `Materials/MI_Sweat_<Name>_<Hash>` | A copy of each material instance between the master and the character's own skin material. The last one carries the face or body mask and the bead cell count measured on that mesh. |

The component finds a sweat material by its name, which is built from the name and path of the original. So if you rename or move a character's skin material, run the command again for that character.

The same calls are available to editor scripts: `unreal.MetaHumanSweatEditorLibrary.add_sweat_to_meshes(meshes)`, `add_sweat_to_actors(actors, add_component)` and `update_sweat_materials()`. Each returns the report as text.

## The component

### Inputs

| Property | Range | Default | Meaning |
|---|---|---|---|
| Sweat Enabled | on/off | on | Off restores the original skin materials and stops the model. From Blueprint use **Set Sweat Enabled**. |
| Effort | 0 to 1 | 0 | How hard the character is working. Sweat builds at *Sweat Build Rate x Effort*. |
| Rain Exposure | 0 to 1 | 0 | 0 dry or under cover, 1 in a downpour. Feeds wetness. |
| Immersion | 0 to 1 | 0 | 0 out of the water, 1 submerged. Feeds wetness quickly. |
| Effort From Speed | on/off | off | Effort also follows the actor's speed: speed / Full Effort Speed. The larger of that and Effort is used. |
| Full Effort Speed | 50 to 2000 cm/s | 500 | The speed that counts as flat out. 500 is about a run; 150 soaks everyone who walks. |
| Initial Sweat | 0 to 1 | 0 | The sweat level at Begin Play. |

### Look

Set for the whole project in **Project Settings > Plugins > MetaHuman Sweat**; turn on **Override Look** on a component to give one character its own.

| Property | Range | Default | Usual range | Meaning |
|---|---|---|---|---|
| Intensity | 0 to 2 | 1 | 0.7 to 1.3 | How strongly a given sweat level shows. 0 hides it. |
| Bead Density | 0.25 to 4 | 1 | 0.6 to 1.6 | Beads per area. 1 is about one bead cell per 6 mm of skin. |
| Bead Size | 0.25 to 3 | 1 | 0.7 to 1.4 | Bead radius. Above 1.5 beads begin to read as spots. |
| Streak Length | 0 to 3 | 1 | 0.6 to 1.6 | How far a drop runs. 1 is about 1 to 2 cm. 0 turns the streaks off. |
| Flow Flip | 0 to 3 | 0 | 0 | Only for a custom body whose drops run uphill: 1 flips U, 2 flips V, 3 both. |

### Pace

Also a project setting, with **Override Rates** per component. Rates are per second and are illustrative, not physiology.

| Property | Range | Default | Meaning |
|---|---|---|---|
| Sweat Build Rate | 0.005 to 0.5 | 0.04 | How fast sweat rises at full effort. |
| Sweat Dry Rate | 0.002 to 0.2 | 0.015 | How fast sweat falls, always. |
| Rain Rate | 0.005 to 1 | 0.08 | How fast rain wets the skin at full exposure. |
| Immersion Rate | 0.05 to 5 | 0.8 | How fast water wets the skin when submerged. |
| Wetness Dry Rate | 0.001 to 0.2 | 0.01 | How fast rain and immersion wetness dries. |

The build and dry rates balance at *build / (build + dry)*, so sweat never reaches 1: it peaks at 0.73 with the defaults. It gets two thirds of the way there in *1 / (build + dry)* seconds (18 s) and, at rest, halves every *0.69 / dry* seconds (46 s). If the peak is below about 0.18 the drops never start.

### Blueprint functions

`Set Sweat Enabled`, `Set Effort`, `Set Rain Exposure`, `Set Immersion`, `Set Sweat Level`, `Set Wetness Level`, `Dry Now`, `Refresh Skin Slots`, `Get Sweat Level`, `Get Wetness Level`, `Get Bound Slot Count`, `Get Sweat Status`.

`Get Sweat Status` returns one line with the levels, the number of skin slots that draw a sweat material and, when that is 0, the reason.

## Project settings

**Project Settings > Plugins > MetaHuman Sweat**

| Setting | Default | Meaning |
|---|---|---|
| Look, Rates | see above | The defaults for every component. |
| Material Folder | `/Game/MetaHumanSweat` | Where the tools write and the component reads. If you change it after materials exist, move the folder in the Content Browser too. |
| Skin Material Names | `M_skin_unified_baked` | Names of the skin master materials the tools recognise. The master must use Material Attributes. |
| Bead Cell Size Cm | 0.6 (0.2 to 2) | Size of one bead cell on the skin. Applies to materials created or refreshed afterwards. |

## Console

| Command | What it does |
|---|---|
| `mhs.Enabled 0` / `1` | Master switch for every component. 0 restores all original materials. |
| `mhs.Debug 1` | One on-screen line per component: levels, effort, bound slots, and why nothing is bound. |
| `mhs.Exert <0-1>` | Sets Effort on every component in the world (1 with no value). |
| `mhs.SetSweat <0-1>` | Jumps every component's sweat level (0.7 with no value). |
| `mhs.SetWetness <0-1>` | Jumps every component's wetness level (0.7 with no value). |
| `mhs.Dry` | Sets both levels and Effort to 0. |
| `mhs.Status` | Logs one status line per component. |

## Troubleshooting

| What you see | Why | What to do |
|---|---|---|
| Nothing changes; status says *no sweat materials for this character's skin* | The sweat versions were not made, or the skin material was renamed or moved since | Select the character and run *Add Sweat to Selected Actors* |
| The report says *No skin material was found* | The character's skin master has another name | Add the master's name to *Skin Material Names*, then run the command again |
| The report says a master *does not use Material Attributes* | An older MetaHuman skin material | Not supported in this version |
| The skin turns matte the moment play starts | Intended: dry skin is matte so that sweat reads as a change | Raise *Initial Sweat*, or turn the component off until it is needed |
| Sweat and beads over the character's underwear | The stock body has underwear painted into the skin texture | Turn *Show Top Underwear* off in MetaHuman Creator before assembling, or dress the character |
| Drops run uphill on a custom body | Its tangent frame is mirrored | Set *Flow Flip* to 1, 2 or 3 |
| A skin tint you drive from Blueprint stops updating | The component replaced your dynamic material instance in the slot (it copied its values once) | Drive the tint before the component binds, or apply it to the material in the slot after Begin Play |
| No sweat in a packaged game | The sweat materials were not cooked | Check that `/Game/MetaHumanSweat` is in *Additional Asset Directories to Cook* |
| Sweat is gone after changing a shader file | The materials hold a copy of the shader code | Run *Update Sweat Materials* |

## Limits

- The pattern lives in the skin's UV layout: trails are cut at UV seams, and drops do not merge or drip off the body.
- Beads fade to a plain sheen when they would be smaller than a couple of pixels, so distant characters show gloss without beads.
- Where the face mesh meets the body mesh the bead pattern changes; the sheen and bead size are matched across it.
- A material instance that is used on both the face mesh and the body mesh (the lowest MetaHuman LODs do this) keeps the mask of its first use.
- Clothing is not affected.

## Changing the look in code

The shader code is in `Resources/Shaders`: `Field.hlsl` makes the beads, streaks and film; the four `Blend*.hlsl` files apply them to base colour, roughness, specular and the normal. Edit, then run **Update Sweat Materials**.

The masks are `Resources/Masks/FaceMask.png` and `BodyMask.png` (1024 x 1024, linear): red is where beads form, green where drops run, blue is a low-frequency patchiness, and alpha below 1 doubles the bead density where the face mesh's UVs are stretched over the neck and collar. To use changed masks, delete the two textures under `/Game/MetaHumanSweat/Textures` and run an *Add Sweat* command again.
