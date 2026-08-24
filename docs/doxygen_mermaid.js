// Mermaid rendering integration for Doxygen HTML output
document.addEventListener("DOMContentLoaded", function() {
    // 1. Transform explicit .mermaid containers
    document.querySelectorAll("div.mermaid, pre.mermaid").forEach(function(el) {
        var text = el.textContent || "";
        var container = document.createElement("div");
        container.className = "mermaid";
        container.textContent = text;
        el.parentNode.replaceChild(container, el);
    });

    // 2. Transform Doxygen markdown code fragments containing mermaid diagrams
    document.querySelectorAll("div.fragment").forEach(function(el) {
        var text = el.textContent || "";
        var trimmed = text.trim();
        if (trimmed.startsWith("mermaid") || trimmed.startsWith("graph ") || 
            trimmed.startsWith("sequenceDiagram") || trimmed.startsWith("classDiagram") || 
            trimmed.startsWith("stateDiagram") || trimmed.startsWith("gantt") ||
            trimmed.startsWith("flowchart")) {
            
            var cleanedText = trimmed.replace(/^mermaid\s*/, "");
            var container = document.createElement("div");
            container.className = "mermaid";
            container.textContent = cleanedText;
            el.parentNode.replaceChild(container, el);
        }
    });

    if (typeof mermaid !== "undefined") {
        mermaid.initialize({
            startOnLoad: true,
            theme: 'default',
            securityLevel: 'loose',
            flowchart: { useMaxWidth: true, htmlLabels: true }
        });
    }
});
