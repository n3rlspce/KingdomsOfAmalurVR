# HUD shader replacements

These four vertex-shader replacement sources accompany the wrist HUD and lower-screen experience/spell HUD. Use them with the matching diagnostic and bridge; preserve the existing geo11 installation and other shader fixes. They belong in the game's `ShaderFixes` directory.

Back up existing replacements before deployment. After replacing these sources while the game is closed, back up and invalidate only the matching automatically converted `ShaderFixesDM/<hash>-vs.txt` and `.bin` cache entries so geo11 regenerates them. Preserve manually authored converted files. The regenerated wrist shaders should contain `AmalurWristClip` and the t117 resource. Compiled caches are intentionally excluded from this repository. The experimental release installer handles these cache backups and invalidations.
