# 浏览器分析实现

Date: 2026-08-13

## Final-review concentrated fix pass



### Fixes

- The reduced stiffness system is now symmetrically scaled from its diagonal before elimination. Pivot tolerance is calculated from the scaled coefficient matrix only, excluding the load vector, and the scaled solution is transformed back to physical displacements. This keeps an independent stiffness-`1` subsystem solvable alongside a stiffness-`1e20` subsystem while preserving singular/mechanism detection.
- An otherwise valid model with no free degrees of freedom now returns `{ ok: false, error }` and no `results` payload.
- Deformation coordinates now use `((coordinate - minimum) / span) * displaySpan`, so a span near `1e-320` never requires an overflowing display-scale factor. Displacements are normalized through their largest finite component before display scaling, keeping generated geometry finite. Deformation-legend font size now tracks `displaySpan`; the fixed CSS size was removed so both tiny and ordinary viewBoxes remain readable.
