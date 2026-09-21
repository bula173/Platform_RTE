# safeAPIFramework - Community Improvement License (CIL)

**Version:** 1.0  
**Effective Date:** 2026-08-02  
**Repository:** https://github.com/[your-org]/safeAPIFramework  

---

## 1. License Summary

**safeAPIFramework** (including documentation, design patterns, code, and assessment templates) is provided under a **Community Improvement License (CIL)** that encourages collaborative development and knowledge sharing within the railway safety engineering community.

---

## 2. Grant of Rights

You are granted the right to:

✅ **Use** — Use safeAPIFramework for any project (commercial or non-commercial)  
✅ **Study** — Study the source code, design, and documentation  
✅ **Modify** — Modify the code and templates for your needs  
✅ **Distribute** — Distribute modified or unmodified versions  
✅ **Deploy** — Deploy to production systems (subject to Section 4)  

---

## 3. Mandatory Contribution Requirements

### 3.1 Primary Requirement: Share Improvements

**If you improve safeAPIFramework, you SHALL contribute those improvements back to the community.**

"Improvements" include:
- Bug fixes
- New features
- Better documentation
- Template enhancements
- Test case improvements
- Performance optimizations
- Security fixes
- safeAPIFramework OSAdapter implementations (RTOS-specific)
- New assessment templates or checklist items

**How to share improvements:**
1. Submit a Pull Request to the official repository
2. Or fork the repository and provide a reference (if you cannot contribute directly)
3. Improvements become part of the official release

**Timeline:** Improvements should be contributed within **6 months** of first use in your project.

### 3.2 Secondary Requirement: Feedback via Issues

**If you cannot contribute code improvements, you MUST provide feedback via GitHub Issues.**

Report:
- Bugs found in safeAPIFramework or templates
- Suggestions for improvement
- Your experience using templates in SIL 4 assessments
- Missing documentation or guidance
- Notified Body feedback (if sharable)

**Frequency:** At least one issue per 6 months of project use.

**Examples of good issues:**
```
Title: "Template 06_HARA missing guidance for concurrent IPC hazards"
Description: "When multiple tasks use IPC simultaneously, race conditions 
not covered in hazard identification section. Suggest adding..."

Title: "safeAPIFramework timer OSAdapter needs timeout mechanism"
Description: "In our VxWorks OSAdapter implementation, we discovered that 
blocking timer operations need timeout protection. Here's what we did..."
```

### 3.3 Exception: Proprietary Workarounds

**Limited exception:** Proprietary workarounds and confidential project-specific adaptations do NOT need to be shared if:

- They are specific to your railway system (not generic improvements)
- They contain sensitive safety-critical logic (e.g., your algorithm)
- Your contract or regulatory context prevents disclosure

**However:** Even confidential adaptations should generate a **generic GitHub Issue** describing the problem and solution approach (without revealing your IP).

**Example:**
```
Title: "safeAPIFramework NVM OSAdapter needs wear-leveling for high-cycle writes"
Description: "In railway real-time systems with frequent config updates, 
flash wear-out becomes an issue. We implemented wear-leveling in our 
OSAdapter. Suggest adding guidance for integrators on this topic."
```

---

## 4. Deployment & Certification Conditions

### 4.1 SIL 4 Projects

If you deploy safeAPIFramework in a **SIL 4 certified railway system**:

1. **Before Deployment:**
   - Contributions (bug fixes, OSAdapter implementations) should be submitted to the project
   - If not possible, create a GitHub Issue describing your verification approach

2. **After Certification:**
   - Share your Safety Case approach (without revealing confidential design)
   - Your Notified Body assessment experience
   - Any safeAPIFramework improvements discovered during verification

3. **Long-term Support:**
   - If safeAPIFramework is integral to your certified system, commit to:
     - Reporting bugs found in production
     - Contributing OSAdapter improvements for your RTOS
     - Participating in security patches (6-month window)

### 4.2 Non-Certified Systems

For research, development, and testing:
- All rights granted in Section 2 apply
- Feedback via Issues encouraged but optional
- Improvements appreciated but not mandatory

---

## 5. Conditions for Distribution

If you distribute safeAPIFramework (modified or unmodified):

✓ **Include this LICENSE** — This license file must be included  
✓ **Preserve Attribution** — Maintain original author credits and file headers  
✓ **Document Changes** — Clearly mark any modifications you made  
✓ **Link Back** — Reference the official repository: https://github.com/[your-org]/safeAPIFramework  
✓ **Pass Along License** — Derivatives must be distributed under this same license  
✓ **Include CHANGELOG** — Document what you changed and why  

---

## 6. Assessment Template Usage

### 6.1 Using the Templates

The **29 SIL 4 assessment templates** (in `docs/templates/`) are specifically designed for railway projects. Using them implies:

1. **You customize them for your project** (required)
2. **You follow EN 50128:2011 in good faith** (required)
3. **You report your experience** (via Issue or PR)

### 6.2 Template Improvements

If you improve the assessment templates:
- **Share improved versions** via Pull Request
- **Document what you improved** and why
- **Real-world experience from your SIL 4 assessment** is valuable feedback

**Examples:**
- "Added new template for SIL 3 projects (simpler)"
- "Template 06_HARA missing guidance for X railway domain"
- "Better examples for integrating safeAPIFramework OSAdapter"
- "Notified Body requested these changes"

---

## 7. GitHub Contribution Workflow

### 7.1 Submitting Improvements

```
Step 1: Fork the repository
        git clone https://github.com/[your-org]/safeAPIFramework.git

Step 2: Create a feature branch
        git checkout -b feature/your-improvement

Step 3: Make changes & commit
        git commit -m "Improve: [description]"

Step 4: Push to your fork & create Pull Request
        git push origin feature/your-improvement
        GitHub: Create Pull Request with description

Step 5: Discuss & iterate
        Project maintainers provide feedback
        You refine the contribution

Step 6: Merge
        Contribution becomes part of official release
```

### 7.2 Creating Issues (Minimum Feedback)

```
Go to: https://github.com/[your-org]/safeAPIFramework/issues

Click: "New Issue"

Provide:
- Title: Clear summary of bug/suggestion
- Description: Detailed explanation
- Your use case: Railway domain, SIL level, timeline
- Suggested fix (if applicable)
- Notified Body feedback (if shareable)
```

---

## 8. No Warranty & Liability

**safeAPIFramework is provided AS-IS with NO WARRANTY.**

### 8.1 Disclaimer

- No guarantee of fitness for any particular purpose
- No guarantee of safety (you must verify)
- No liability for damages arising from use
- No guarantee of security or performance

### 8.2 Your Responsibility

**You are responsible for:**
- Verifying safeAPIFramework for your use case
- Performing all required static analysis and testing
- Conducting formal assessment via Notified Body
- Obtaining regulatory approval from railway authorities
- Managing residual risks in your system

**This license does NOT provide:**
- Certification by any authority
- Warranty of SIL compliance
- Legal guarantee of safety
- Support or maintenance (except community-driven)

---

## 9. Community Governance

### 9.1 Contribution Recognition

Contributors are recognized via:
- Git commit history (author name preserved)
- CONTRIBUTORS.md file
- Release notes for major contributions
- GitHub profile visibility

### 9.2 Maintainer Role

- **Maintainer:** Curator of safeAPIFramework, accepts/reviews contributions
- **Contributors:** Community members improving the project
- **Users:** Deploy safeAPIFramework in their systems

### 9.3 Dispute Resolution

If disputes arise about licensing or contributions:
1. Open an Issue on GitHub
2. Discussion happens in public
3. Maintainer makes final decision
4. Decision is binding but can be appealed

---

## 10. Special Cases

### 10.1 Academic Use

**Universities & research institutions:**
- May use safeAPIFramework freely
- Encouraged to publish improvements (peer review = contribution)
- Should cite safeAPIFramework in academic papers
- Issues & PRs appreciated but optional

### 10.2 Embedded in Products

If you embed safeAPIFramework in a commercial product:
- Share back any improvements to safeAPIFramework itself
- Proprietary extensions around safeAPIFramework don't need to be shared
- Document your use case (Issue or GitHub Discussion)

**Example:**
```
Your product:
├── safeAPIFramework (shared community version)
├── Your proprietary railway algorithm (yours to keep)
└── Your RTOS OSAdapter (share with community!)
```

### 10.3 Regulatory Compliance

If your regulatory authority (railway certification body) requires you to NOT share certain information:
- Document this in an Issue (explain the restriction, not the secret)
- Contributors understand regulatory constraints
- Work with maintainers on what can/cannot be shared

---

## 11. License Terms

### 11.1 Copyleft Provision

**"Improvements must be shared" = Copyleft**

Derived works (modifications) must also be distributed under this same license.

This ensures:
- Improvements benefit everyone
- No "closed-source forks" that hide better solutions
- Railway safety community benefits from your verification work

### 11.2 Attribution

All files must retain:
```c
// safeAPIFramework - Community Improvement License (CIL) v1.0
// Maintained at: https://github.com/[your-org]/safeAPIFramework
// See LICENSE.md for terms
```

### 11.3 License Violations

If someone violates this license:
1. Maintainer sends a notice (via Issue/email)
2. 30-day cure period to comply
3. If not resolved, legal action may follow (per applicable jurisdiction)

---

## 12. Termination

This license grants you rights **perpetually**, but:

- Rights terminate if you sue for patent infringement related to safeAPIFramework
- Rights terminate if you violate Sections 3 or 5 without remediation
- No termination for non-commercial or research use

---

## 13. Versions

This license may be updated by maintainer:

| Version | Date | Change |
|---------|------|--------|
| 1.0 | 2026-08-02 | Initial Community Improvement License |

**You automatically use the latest version when you update safeAPIFramework.**

---

## 14. How to Comply

### Quick Compliance Checklist

- [ ] **Read this license** (you're doing it!)
- [ ] **Preserve LICENSE.md** in any distribution
- [ ] **Document your changes** in git commits
- [ ] **Share improvements** via Pull Request within 6 months
- [ ] **Or submit Issues** describing your use case & findings
- [ ] **Cite safeAPIFramework** if you publish your work
- [ ] **Report bugs** you find in production

**That's it!** Simple, reasonable, community-focused.

---

## 15. Contact

**Questions about this license?**

1. **Create a GitHub Issue** → Public discussion
2. **Email Maintainer** → Private discussion (if needed)
3. **Check CONTRIBUTING.md** → Community guidelines

---

## 16. Acknowledgments

This license is inspired by:
- **GPL v3** (copyleft principle: improvements shared back)
- **Apache 2.0** (patent clarity)
- **Community-driven development** (Linux, PostgreSQL, etc.)

**Tailored for:** Railway safety-critical systems where knowledge sharing saves lives.

---

## 17. Your Rights Summary

| Right | Community Use | Commercial Use | SIL 4 Deployment |
|-------|---|---|---|
| **Use freely** | ✅ Yes | ✅ Yes | ✅ Yes (verify yourself) |
| **Modify** | ✅ Yes | ✅ Yes | ✅ Yes |
| **Distribute** | ✅ Yes (share improvements) | ✅ Yes (share improvements) | ✅ Yes (share improvements) |
| **Deploy** | ✅ Yes | ✅ Yes | ✅ Yes (formal assessment required) |
| **Keep proprietary** | ❌ No (share back) | ⚠️ Limited (algorithm yes, safeAPIFramework no) | ⚠️ Very limited |

---

**ACCEPT & AGREE:** By using safeAPIFramework, you agree to this license.

**Questions?** Open an Issue: https://github.com/[your-org]/safeAPIFramework/issues

**Ready to contribute?** Submit a Pull Request: https://github.com/[your-org]/safeAPIFramework/pulls

---

**Last Updated:** 2026-08-02  
**License Version:** 1.0  
**Status:** ACTIVE
