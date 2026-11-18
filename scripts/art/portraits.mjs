// Build-time only; portraits, card art and running puppet use the same geometry.
import fs from 'node:fs/promises';
import { BUILTINS } from '../../core/repertoire.mjs';
import { drawChibi } from '../../art-src/chibi/puppet.mjs';
for (const { id } of BUILTINS)
  for (const portrait of [false, true])
    await fs.writeFile(
      new URL(
        `../../public/art/${id}${portrait ? '-face' : ''}.svg`,
        import.meta.url,
      ),
      drawChibi({ skin: id, portrait }) + '\n',
    );
