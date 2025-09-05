# Testing Strum Note Overlap Behavior

## Current Implementation Analysis

The Strum operator currently only modifies **StartTime** of notes, not their length. This means:

### What happens when notes overlap due to strum:

1. **Notes maintain their original length** - only start times are shifted
2. **Ardour's existing overlap resolution handles conflicts** - via `MidiModel::resolve_overlaps_unlocked()`
3. **Overlap behavior follows the current InsertMergePolicy**

### Overlap Resolution Policies (from midi_model.cc):
- `InsertMergeReplace` - New note replaces overlapping ones
- `InsertMergeTruncateExisting` - Existing notes get shortened
- `InsertMergeTruncateAddition` - New note gets shortened
- `InsertMergeExtend` - Notes get extended to cover overlap
- `InsertMergeRelax` - No overlap resolution (allows overlaps)

### Test Scenario:
```
Original notes:  [Note1: 0-500ms] [Note2: 600-1100ms]
After strum:     [Note1: 0-500ms] [Note2: 32ms-532ms] <- OVERLAP!
                           ^overlap region^
```

### Current Behavior:
The overlap is handled by Ardour's existing MIDI model conflict resolution, 
following whatever InsertMergePolicy is currently set.

### Potential Issues:
1. **Unintended note deletion** - if policy is Replace
2. **Unexpected note truncation** - if policy truncates
3. **Musical timing disruption** - overlaps may not sound as intended

### Possible Solutions:
1. **Keep current behavior** - rely on existing overlap resolution
2. **Add overlap detection** - warn user or prevent overlapping strums  
3. **Smart length adjustment** - automatically adjust note lengths to prevent overlaps
4. **Strum-specific policy** - add parameter for how to handle strum overlaps
