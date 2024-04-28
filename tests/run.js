#!/usr/bin/env deno run --allow-run

function av_randstr(minlen, maxlen) {
   return "wow" 
}

async function generate(outputfile,
                   title,
                   album,
                   artist,
                   cover,
                   duration = 30) {
    const maxlen = 32
    const maxlen_text = 64
    const cmd = new Deno.Command("/opt/homebrew/bin/ffmpeg", { 
                    args:  ["-y",
                              "-f", "lavfi",
                              "-i", `anullsrc=duration=${duration}`,
                              "-c:a", "aac", "-shortest"
                   ] +
                   (cover == null ? [] : ["-c:v", "copy", "-i", cover]) +
                   [
                       "-metadata", `title="${title == null ? av_randstr(1, maxlen) : title}"`,
                       "-metadata", `album="${album == null ? av_randstr(1, maxlen) : album}"`,
                       "-metadata", `artist="${artist == null ? av_randstr(1, maxlen) : artist}"`,
                       "-metadata", `comment="${av_randstr(1, maxlen_text)}"`,
                       "-metadata", `description="${av_randstr(1, maxlen_text)}"`,
                       "-metadata", `genre="${av_randstr(1, maxlen)}"`,
                       "-metadata", `composer="${av_randstr(1, maxlen)}"`,
                       "-metadata", `copyright="${av_randstr(1, maxlen)}"`,
                       "-metadata", `synopsis="${av_randstr(1, maxlen_text)}"`,
                       outputfile
                   ]
                })
    const { code, stdout, stderr } = await cmd.output();
    console.log(stdout);
    console.log(new TextDecoder().decode(stderr));
    console.log("return code: " + code)
}

async function main() {
   await generate("red.mp4")
}

await main()
