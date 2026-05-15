#include "frontend.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// The HTML page is assembled at runtime so we can inject storage_info and
// file_list. The heavy parts (JSZip, conversion logic) are compile-time
// constants split into prefix / suffix around the two injected strings.
// ---------------------------------------------------------------------------

static const char HTML_PREFIX[] = R"RSVP(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>RSVP Reader</title>
<style>
  body { font-family: sans-serif; max-width: 600px; margin: 2em auto; padding: 0 1em; }
  h2 { margin-bottom: 0.3em; }
  #status { color: #555; font-style: italic; margin-top: 0.5em; min-height: 1.2em; }
  input[type=file] { margin-bottom: 0.5em; }
  ul { padding-left: 1.2em; }
</style>
</head>
<body>
<h2>RSVP Reader</h2>
<form id="uploadForm">
  <input type="file" id="fileInput" accept=".txt,.epub" required><br>
  <div id="status"></div><br>
  <input type="submit" id="submitBtn" value="Upload">
</form>
<hr>
<h3>Storage</h3><p>)RSVP";

static const char HTML_MIDDLE[] = R"RSVP(</p>
<hr>
<h3>Files</h3><ul>)RSVP";

static const char HTML_SUFFIX[] = R"RSVP(</ul>
<script src="/jszip.js"></script>
<script>
)RSVP";

// JSZip minified will be injected here via a separate constant
static const char HTML_JSZIP_END[] = R"RSVP(
(function(){
  var form   = document.getElementById('uploadForm');
  var input  = document.getElementById('fileInput');
  var status = document.getElementById('status');
  var btn    = document.getElementById('submitBtn');

  function setStatus(msg){ status.textContent = msg; }

  function uploadBlob(blob, filename){
    console.log('[upload] blob type:', blob.type, 'size:', blob.size, 'filename:', filename);
    setStatus('Uploading ' + filename + '...');
    var fd = new FormData();
    fd.append('file', blob, filename);
    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/upload', true);
    xhr.onload = function(){
      console.log('[upload] response status:', xhr.status, 'body:', xhr.responseText);
      if(xhr.status === 200){
        setStatus('Done! Reloading...');
        setTimeout(function(){ location.reload(); }, 1200);
      } else {
        setStatus('Upload failed: ' + xhr.status);
        btn.disabled = false;
      }
    };
    xhr.onerror = function(){ console.log('[upload] XHR error'); setStatus('Upload error.'); btn.disabled = false; };
    xhr.send(fd);
    console.log('[upload] XHR sent');
  }

  // Strip HTML tags and decode entities to plain text
  function htmlToText(html){
    return html
      .replace(/<[^>]*>/g, ' ')
      .replace(/&nbsp;/g, ' ')
      .replace(/&amp;/g, '&')
      .replace(/&lt;/g, '<')
      .replace(/&gt;/g, '>')
      .replace(/&quot;/g, '"')
      .replace(/&#39;/g, "'")
      .replace(/[ \t]+/g, ' ')
      .replace(/\n[ \t]+/g, '\n')
      .replace(/\n{3,}/g, '\n\n')
      .trim();
  }

  // Resolve a path relative to a base directory
  function resolvePath(base, href){
    var clean = href.split('?')[0].split('#')[0];
    if(clean.indexOf('/') === 0) return clean.substring(1);
    return base + clean;
  }

  // Extract the anchor fragment from an href
  function anchorOf(href){
    var i = href.indexOf('#');
    return i >= 0 ? href.substring(i + 1) : '';
  }

  // Extract text from an HTML file starting at an anchor id, stopping at the next anchor id
  // If startAnchor is empty, start from beginning. If endAnchor is empty, go to end.
  function extractSlice(html, startAnchor, endAnchor){
    var body = html;
    // Grab body content if present
    var bm = html.match(/<body[^>]*>([\s\S]*)<\/body>/i);
    if(bm) body = bm[1];

    if(startAnchor){
      // Find the element with id=startAnchor
      var re = new RegExp('<[^>]+\\bid=["\']' + startAnchor.replace(/[.*+?^${}()|[\]\\]/g,'\\$&') + '["\'][^>]*>', 'i');
      var idx = body.search(re);
      if(idx >= 0) body = body.substring(idx);
    }

    if(endAnchor){
      var re2 = new RegExp('<[^>]+\\bid=["\']' + endAnchor.replace(/[.*+?^${}()|[\]\\]/g,'\\$&') + '["\'][^>]*>', 'i');
      var idx2 = body.search(re2);
      if(idx2 >= 0) body = body.substring(0, idx2);
    }

    return htmlToText(body);
  }

  // Parse toc.ncx and return [{title, file, anchor}] in playOrder
  function parseNcx(ncxXml){
    var entries = [];
    var re = /<navPoint[^>]*playOrder="(\d+)"[^>]*>([\s\S]*?)<\/navPoint>/gi;
    // ncx can be nested — flatten by just matching all navPoints
    var allNp = [];
    var m;
    var npRe = /<navPoint[\s\S]*?<\/navPoint>/gi;
    // Use a simpler line-by-line approach: find all navLabel/text + content src pairs
    var labelRe = /<navLabel[^>]*>\s*<text[^>]*>([\s\S]*?)<\/text>/gi;
    var srcRe   = /<content[^>]*\bsrc="([^"]+)"/gi;
    var labels = [], srcs = [];
    while((m = labelRe.exec(ncxXml)) !== null) labels.push(m[1].trim());
    while((m = srcRe.exec(ncxXml))   !== null) srcs.push(m[1]);
    for(var i = 0; i < Math.min(labels.length, srcs.length); i++){
      var href = srcs[i];
      entries.push({
        title:  htmlToText(labels[i]),
        file:   href.split('#')[0],
        anchor: anchorOf(href)
      });
    }
    return entries;
  }

  function parseEpub(arrayBuffer, filename){
    setStatus('Unpacking EPUB...');
    JSZip.loadAsync(arrayBuffer).then(function(zip){

      return zip.file('META-INF/container.xml').async('string').then(function(containerXml){
        var m = containerXml.match(/full-path="([^"]+\.opf)"/i);
        if(!m) throw new Error('Cannot find OPF in container.xml');
        var opfPath = m[1];
        var opfDir  = opfPath.indexOf('/') >= 0 ? opfPath.substring(0, opfPath.lastIndexOf('/') + 1) : '';
        return zip.file(opfPath).async('string').then(function(opfXml){
          return { zip: zip, opfXml: opfXml, opfDir: opfDir };
        });
      });

    }).then(function(ctx){
      var zip    = ctx.zip;
      var opfXml = ctx.opfXml;
      var opfDir = ctx.opfDir;

      // Build id→href manifest
      var manifest = {};
      var manifestType = {};
      var mRe = /<item\s[^>]+>/gi, m;
      while((m = mRe.exec(opfXml)) !== null){
        var tag   = m[0];
        var idM   = tag.match(/\bid="([^"]+)"/i);
        var hrefM = tag.match(/\bhref="([^"]+)"/i);
        var typeM = tag.match(/\bmedia-type="([^"]+)"/i);
        var propM = tag.match(/\bproperties="([^"]+)"/i);
        if(idM && hrefM){
          manifest[idM[1]]     = hrefM[1];
          manifestType[idM[1]] = (typeM ? typeM[1] : '') + '|' + (propM ? propM[1] : '');
        }
      }

      // Find toc.ncx or nav.xhtml
      var ncxId = null, navId = null;
      // OPF may reference NCX via <spine toc="...">
      var spineTocM = opfXml.match(/<spine[^>]+\btoc="([^"]+)"/i);
      if(spineTocM) ncxId = spineTocM[1];
      for(var id in manifestType){
        if(manifestType[id].indexOf('dtbncx') >= 0) ncxId = ncxId || id;
        if(manifestType[id].indexOf('nav') >= 0)    navId = id;
      }

      // Spine order (fallback)
      var spineIds = [];
      var spineRe = /<itemref[^>]+idref="([^"]+)"/gi;
      while((m = spineRe.exec(opfXml)) !== null) spineIds.push(m[1]);
      if(spineIds.length === 0) throw new Error('Empty spine in OPF');

      // Spine href list in order (bare paths, no fragment)
      var spineHrefs = spineIds.map(function(id){
        var href = manifest[id];
        return href ? href.split('?')[0].split('#')[0] : null;
      }).filter(Boolean);

      // Fetch all spine files into a map keyed by bare href
      var spineFiles = {};
      var fetchPromises = spineHrefs.map(function(href){
        var fullPath = resolvePath(opfDir, href);
        var f = zip.file(fullPath);
        if(!f) return Promise.resolve(null);
        return f.async('string').then(function(html){ spineFiles[href] = html; });
      });

      // Also fetch NCX if available
      var ncxPromise = Promise.resolve(null);
      if(ncxId && manifest[ncxId]){
        var ncxPath = resolvePath(opfDir, manifest[ncxId]);
        var ncxFile = zip.file(ncxPath);
        if(ncxFile) ncxPromise = ncxFile.async('string');
      }

      return Promise.all([Promise.all(fetchPromises), ncxPromise]).then(function(results){
        var ncxXml = results[1];

        // Parse ToC entries from NCX
        var tocEntries = ncxXml ? parseNcx(ncxXml) : [];
        console.log('[epub] NCX entries:', tocEntries.length);

        // If NCX has <= 1 entry, fall back to one entry per spine file
        if(tocEntries.length <= 1){
          console.log('[epub] falling back to spine');
          tocEntries = spineHrefs.map(function(href){
            return { title: '', file: href, anchor: '' };
          });
        }

        setStatus('Building text from ' + tocEntries.length + ' ToC entries...');

        // Build a spine index map: href -> position in spine
        var spineIndex = {};
        spineHrefs.forEach(function(href, i){ spineIndex[href] = i; });

        var parts = tocEntries.map(function(entry, idx){
          var startFile   = entry.file;
          var startAnchor = entry.anchor;

          // Next ToC entry determines where this chapter ends
          var nextEntry   = tocEntries[idx + 1];
          var endFile     = nextEntry ? nextEntry.file   : null;
          var endAnchor   = nextEntry ? nextEntry.anchor : '';

          var startIdx = spineIndex[startFile];
          if(startIdx === undefined) return '';

          // Collect all spine files from startFile up to (not including) endFile
          var chunkHtml = '';
          for(var si = startIdx; si < spineHrefs.length; si++){
            var href = spineHrefs[si];
            if(endFile && href === endFile && si !== startIdx){
              // Last file of this chapter: slice up to endAnchor
              var html = spineFiles[href];
              if(html) chunkHtml += extractSlice(html, '', endAnchor);
              break;
            }
            if(endFile && si > startIdx && spineIndex[endFile] !== undefined && si >= spineIndex[endFile]) break;
            var html = spineFiles[href];
            if(!html) continue;
            if(si === startIdx){
              chunkHtml += extractSlice(html, startAnchor, (endFile === startFile ? endAnchor : ''));
            } else {
              chunkHtml += '\n' + extractSlice(html, '', '');
            }
            // Stop if this was the last file before the next chapter's file
            if(endFile && href !== endFile && spineHrefs[si + 1] === endFile && !endAnchor) break;
          }

          var text = chunkHtml.replace(/\n{3,}/g, '\n\n').trim();
          if(!text) return '';
          return entry.title ? '# ' + entry.title + '\n\n' + text : text;
        });

        var fullText = parts.filter(function(p){ return p.trim(); }).join('\n\n');
        console.log('[epub] final text length:', fullText.length);
        var txtName = filename.replace(/\.epub$/i, '.txt');
        var blob = new Blob([fullText], { type: 'text/plain' });
        uploadBlob(blob, txtName);
      });

    }).catch(function(err){
      setStatus('EPUB error: ' + err.message);
      console.error('[epub] error:', err);
      btn.disabled = false;
    });
  }

  form.addEventListener('submit', function(e){
    e.preventDefault();
    var file = input.files[0];
    console.log('[submit] file:', file ? file.name : 'none', 'size:', file ? file.size : 0, 'type:', file ? file.type : '');
    if(!file) return;
    btn.disabled = true;

    if(file.name.toLowerCase().endsWith('.epub')){
      setStatus('Reading EPUB...');
      var reader = new FileReader();
      reader.onload = function(ev){
        console.log('[submit] FileReader loaded, byteLength:', ev.target.result.byteLength);
        parseEpub(ev.target.result, file.name);
      };
      reader.onerror = function(){ setStatus('Cannot read file.'); btn.disabled = false; };
      reader.readAsArrayBuffer(file);
    } else {
      uploadBlob(file, file.name);
    }
  });
})();
</script>
</body>
</html>
)RSVP";


// Streams the page in chunks so no large DRAM buffer is needed.
// Caller must have already called server.setContentLength(CONTENT_LENGTH_UNKNOWN)
// and server.send(200, "text/html", "").
void frontend_send_html(void (*sendChunk)(const char*),
                        const char* storage_info,
                        const char* file_list) {
    sendChunk(HTML_PREFIX);
    sendChunk(storage_info);
    sendChunk(HTML_MIDDLE);
    sendChunk(file_list);
    sendChunk(HTML_SUFFIX);
    sendChunk(HTML_JSZIP_END);
}
