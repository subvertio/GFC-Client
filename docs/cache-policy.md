# Cache policy

The app is built for phones with 3 to 4 GB of RAM and limited storage. Downloaded
content is **session-only, owned by the card that shows it, and deleted when
the user moves on**. Re-fetching on scroll-back is the accepted trade-off:
storage, memory and privacy matter more than bandwidth.

## One owner

`UGFCCacheSubsystem` (`Source/GFCApi/Public/GFCCacheSubsystem.h`) is the only
code that downloads media or writes cache files. Its bookkeeping lives in
`FGFCCacheStore` (`GFCCacheStore.h`), a plain C++ class covered by the
`GFC.Mobile.Cache` automation test.

| What | Where | Lifetime |
|---|---|---|
| Thumbnails | Memory only: decoded textures in a bounded LRU | Until evicted, trimmed or logout |
| Full images, video, audio, 3D models | Temp files in the app's private cache folder | Until the card is left, a trim, or logout |
| Session token, settings, known wallets | `Saved/SaveGames/GFCSession.sav` (token encrypted: DPAPI on Windows, Keystore TODO on Android) | Separate from the cache, never in the cache folder |

**Cache folder:** Android uses `Context.getCacheDir()/GFCCache`, which is
private to the app and which the OS may clear when storage runs low. PC uses
`Saved/GFCCache`. Never shared or external storage. As a guard, the store
refuses to work on any folder not named `GFCCache`.

## Caps

All in Project Settings > Game > GFC API (`Config/DefaultGame.ini`).

| Cap | Default | Enforced by |
|---|---|---|
| Disk, all temp media | 200 MB (`MaxDiskCacheMB`) | Other owners' files are deleted oldest first; a single file over the cap is refused |
| Thumbnails in memory | 60 items / 48 MB decoded (`MaxThumbnails`, `MaxThumbnailMB`) | LRU; thumbnails of visible cards are evicted last |
| Live heavy items | 1 (the card being viewed) | Starting another card's 3D, image or media load releases the previous one first |
| Free space floor | 300 MB (`MinFreeDiskMB`) | Downloads that would leave less free space are refused with a friendly message |
| Model file | 50 MB (`MaxModelMB`) | Checked before download (seed size), while downloading (aborts past the cap) and in the parser |
| Full image file | 20 MB (`MaxImageMB`) | Same |
| Video/audio file | half the disk cap | Same |
| Mobile data prompt | above 10 MB (`CellularConfirmMB`) | The detail screen asks before downloading on mobile data |

## Cleanup triggers

| Trigger | What happens |
|---|---|
| **Card scrolled away** (feed) | After `OffscreenGraceSeconds` (0.75 s, so scroll jitter doesn't thrash) the card drops its thumbnail brush and releases its owner key `card:<id>`. Scrolling back re-requests it (from memory if still cached, otherwise re-downloaded). |
| **Seed closed** (detail screen hidden, back, another seed) | The 3D actor, procedural mesh, materials, textures and render target are destroyed, the media player is closed, the in-flight download is cancelled, and the temp file is deleted (`ReleaseOwner("seed:<id>")`). A garbage collection is requested so GPU and RAM memory come back right away. |
| **App to background / low memory** | `FCoreDelegates::ApplicationWillEnterBackgroundDelegate`, `GetMemoryTrimDelegate` and `ApplicationShouldUnloadResourcesDelegate`: everything is released except thumbnails of visible cards. |
| **App start** | Every file in the cache folder is an orphan from a crash or kill, so all are deleted. |
| **Logout** | Everything is wiped (files, memory, heavy item); the token is revoked on the node and cleared locally. |
| **App exit** | The cache is wiped (session-only). |

## Checking it on a device

Console commands, in development builds (`~` on PC; on Android, a four-finger tap opens the console):

- `gfc.cache`: prints usage, e.g. `disk 0.2 MB in 1 files | memory 0.8 MB in 3 items (3 visible) | live: seed:0b1d…`
- `gfc.cache.overlay 1`: shows the same line on screen, refreshed twice a second. `0` hides it.
- `gfc.cache.wipe`: deletes everything now.

### Memory and disk return to baseline

The automated part is `GFC.Mobile.Cache`: disk goes back to zero files after a
heavy item is viewed and released. For the real app, there is a
development-only check:

```powershell
.\Scripts\Run.ps1 -Node http://127.0.0.1:8100 -Screen "seed:<3d seed id>"
```

Add `-GFCAutoView -GFCAutoBackAfter=8` to the command line (see
`Scripts/Run.ps1`). The app opens the seed, loads the model, leaves the card
8 s later, and logs three `GFC lifecycle` lines (model ready, viewing, 3 s
after leaving) with process memory and cache usage. After leaving, the cache
should show `disk 0.0 MB in 0 files | live: none` and the `Saved/GFCCache`
folder should be empty. Process memory should drop back towards the
before-viewing figure; engine pools mean it doesn't return byte-for-byte.

On a phone: open a 3D seed, run `gfc.cache.overlay 1`, go back, and watch disk
and live go to zero. Android Studio's Memory Profiler shows the native and
graphics memory drop.

## Future: "keep offline"

`UGFCSeedDetailScreen::ShouldKeepOffline()` is the hook for an opt-in
offline copy of bookmarked seeds (`POST /seeds/{id}/bookmark`). It returns
false; nothing is kept today. If it's built, offline copies need their own
cap and folder, shown separately in `gfc.cache`.
