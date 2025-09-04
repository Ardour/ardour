# Response: Strum Note Overlap Behavior

## Quick Answer:
**Notes can overlap after strumming, and this is handled by Ardour's existing MIDI overlap resolution system.**

## Technical Details:

### Current Behavior:
1. **Strum only modifies StartTime** - note lengths remain unchanged
2. **Overlaps are resolved by `MidiModel::resolve_overlaps_unlocked()`**
3. **Default policy is `InsertMergeRelax`** - which allows overlaps to exist

### Default Configuration:
```cpp
// From session_configuration_vars.inc.h line 60:
CONFIG_VARIABLE (InsertMergePolicy, insert_merge_policy, "insert-merge-policy", InsertMergeRelax)
```

### What Happens in Practice:

**Example scenario:**
```
Original:   [Note1: 0.0-0.5 beats] [Note2: 1.0-1.5 beats]
Strum +32:  [Note1: 0.0-0.5 beats] [Note2: 1.032-1.532 beats]  ← Safe

But with closer notes:
Original:   [Note1: 0.0-0.5 beats] [Note2: 0.4-0.9 beats] 
Strum +32:  [Note1: 0.0-0.5 beats] [Note2: 0.432-0.932 beats] ← OVERLAP!
```

### Current Resolution:
- **InsertMergeRelax (default)**: Overlaps are allowed, both notes play
- **InsertMergeReplace**: New note position replaces overlapping notes  
- **InsertMergeTruncateExisting**: Original notes get shortened
- **InsertMergeTruncateAddition**: Strummed notes get shortened

## Testing:

**To test overlap behavior:**
1. Create two quarter notes very close together (< 32 ticks apart)
2. Select both and apply strum forward  
3. Check if notes overlap and how Ardour handles it

**The behavior follows whatever the user's current InsertMergePolicy setting is.**

## Recommendation:
Current implementation is **correct** - it delegates overlap handling to Ardour's established MIDI conflict resolution system, maintaining consistency with other MIDI operations.
