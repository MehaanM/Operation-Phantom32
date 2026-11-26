# Legal Notice

## Read This Before Using Phantom32

This document sets out the legal terms under which Phantom32 may be used. By
downloading, building, flashing, or operating this software you agree to these
terms. If you do not agree, do not use the software.

---

## 1. What This Software Does

Phantom32 is firmware for ESP32 microcontrollers that enables:

- Transmission of raw 802.11 (Wi-Fi) management and data frames, including
  deauthentication frames, probe requests, beacon frames, and authentication
  frames.
- Creation of rogue access points (AP) and captive portals.
- Passive capture of Wi-Fi management frames from nearby devices, including
  probe requests and EAPOL frames.
- Bluetooth Low Energy advertisement injection.
- Network traffic analysis and device fingerprinting on networks the operator
  controls.

These capabilities are powerful. Many of them are illegal when directed at
networks, devices, or users without authorization.

---

## 2. Authorized Use Only

**You may only operate Phantom32 in environments where you have explicit,
written authorization from the owner of every network, device, and system
that may be affected.**

Authorized use includes:

- Testing wireless infrastructure on networks you personally own.
- Penetration testing engagements with a signed statement of work or written
  authorization letter from the client, covering the specific systems under test.
- Academic research conducted under an Institutional Review Board (IRB)
  approval or equivalent ethics review that covers wireless security research.
- Capture-the-flag (CTF) competitions where the event rules explicitly permit
  use of deauthentication, rogue AP, and frame injection tools within the
  competition environment.
- Controlled laboratory environments with no live third-party clients present.

---

## 3. Prohibited Use

The following uses are prohibited under all circumstances, regardless of intent:

- Sending deauthentication or disassociation frames to any device connected
  to a network you do not own or do not have explicit written authorization to
  test.
- Operating a rogue access point or captive portal in any location accessible
  to third parties without their knowledge.
- Capturing Wi-Fi credentials, EAPOL handshakes, or probe request data from
  devices you do not own.
- Interfering with Wi-Fi communications at airports, hospitals, hotels,
  conference centers, public spaces, or any shared wireless environment.
- Using Phantom32 for any denial-of-service attack, targeted or broadcast,
  against devices, networks, or infrastructure on the public internet or on
  any network you do not own.
- Using captured credentials, device information, or network data for any
  purpose other than the authorized security assessment for which they were
  collected.
- Distributing firmware modified to remove safety limits or rate limiting
  controls for the purpose of causing harm.

---

## 4. Applicable Law by Jurisdiction

The legal framework governing the use of wireless security research tools
varies by country. The following is a non-exhaustive summary. It is not legal
advice. Consult a licensed attorney in your jurisdiction before conducting
any security testing.

### United States

**Computer Fraud and Abuse Act (CFAA), 18 U.S.C. 1030**
Unauthorized access to computer systems, including transmitting commands that
damage or interfere with the operation of a system, is a federal criminal
offense. Deauthentication attacks against third-party networks can constitute
interference with protected computers under CFAA.

**Electronic Communications Privacy Act (ECPA), 18 U.S.C. 2511**
Intercepting wire, oral, or electronic communications without authorization is
a federal criminal offense. Passive capture of probe requests and EAPOL frames
from third-party devices may constitute interception under ECPA.

**Federal Communications Commission (FCC) Rules, 47 C.F.R.**
Intentional interference with licensed radio communications is prohibited under
FCC rules and may result in civil or criminal penalties. Wi-Fi operates in
unlicensed bands under Part 15, but intentional interference with other Part 15
devices can still violate FCC rules.

**State Laws**
Most U.S. states have their own computer crime statutes (e.g., California Penal
Code 502) that may apply in addition to federal law.

### European Union

**Directive on Attacks Against Information Systems (2013/40/EU)**
Member states are required to criminalize unauthorized access to information
systems and illegal interception of computer data. Penetration testing without
authorization may constitute an offense under implementing legislation in any
EU member state.

**General Data Protection Regulation (GDPR), Regulation (EU) 2016/679**
Capturing data that can identify individual devices (MAC addresses, probe
SSIDs) may constitute processing of personal data under GDPR. Processing
personal data without a lawful basis is prohibited. Art. 83 provides for
administrative fines of up to 20 million EUR or 4% of global annual turnover.

**Specific Member State Laws (examples)**
- Germany: Strafgesetzbuch (StGB) SS 202a (unauthorized data access),
  SS 303a (data interference), SS 303b (computer sabotage).
- United Kingdom: Computer Misuse Act 1990 (as amended); unauthorized access
  to computer material is a criminal offense. The UK is no longer an EU member
  but maintains equivalent legislation.
- France: Code Penal Art. 323-1 through 323-7 (fraudulent access to or
  interference with an automated data processing system).
- Netherlands: Wetboek van Strafrecht Art. 138ab (computer intrusion).

### Canada

**Criminal Code of Canada, R.S.C. 1985, c. C-46**
Section 342.1 (unauthorized use of computer) and Section 430(1.1) (mischief
in relation to computer data) may apply to unauthorized use of deauthentication
or frame injection against third-party networks.

**Personal Information Protection and Electronic Documents Act (PIPEDA)**
Collecting personal information (including device identifiers) without
knowledge or consent may violate PIPEDA.

### Australia

**Criminal Code Act 1995, Schedule 1**
Division 477 (unauthorized access, modification, or impairment of data) and
Division 478 (unauthorized access to, or modification of, restricted data) may
apply. State and territory computer crime laws also apply.

**Privacy Act 1988**
Collection of personal information without consent may violate the Australian
Privacy Principles.

### India

**Information Technology Act, 2000 (IT Act)**
Section 43 (unauthorized access, download, or damage to computer systems) and
Section 66 (computer related offenses) may apply. Unauthorized interception of
data transmitted through a computer resource is also an offense under Section
69B read with the Information Technology (Procedure and Safeguards for
Interception, Monitoring and Decryption of Information) Rules, 2009.

### Singapore

**Computer Misuse Act (Cap. 50A)**
Unauthorized access to computer material and unauthorized interference with
computer functions are criminal offenses.

### Other Jurisdictions

Most countries with modern computer crime legislation have equivalent
prohibitions on unauthorized access and interference. If your jurisdiction is
not listed above, assume that unauthorized use of the capabilities in this
firmware is a criminal offense and seek legal advice before proceeding.

---

## 5. Authorization Documentation

If you are conducting authorized security testing, maintain the following
records for the duration of the engagement and for at least five years
afterward:

- A signed contract or statement of work specifying the scope of authorized
  testing (which networks, which devices, which time window).
- Written authorization from the owner of each in-scope network or system.
  An email from an authorized representative with their full name, role, and
  contact information at minimum.
- A copy of your testing methodology showing which tools were used and when.
- Contact information for a law enforcement liaison at the client organization
  in the event of an incident.

Carry a copy of your authorization letter when conducting on-site testing.
If challenged by law enforcement or property management, present this
documentation immediately. Do not continue testing until authorization is
re-confirmed.

---

## 6. Responsible Disclosure

If you discover a vulnerability in a third-party system while conducting
authorized testing using this firmware, you are responsible for disclosing it
responsibly to the affected organization. Do not:

- Exploit the vulnerability beyond what is necessary to confirm its existence.
- Access, copy, or exfiltrate data beyond proof-of-concept.
- Publish details publicly before the affected organization has had a
  reasonable opportunity to remediate (typically 90 days).

Follow the affected organization's responsible disclosure or bug bounty policy
if one exists. If not, contact their security team directly at their published
security contact address.

---

## 7. No Warranty

This software is provided "as is" without warranty of any kind, express or
implied, including but not limited to the warranties of merchantability,
fitness for a particular purpose, and noninfringement. The entire risk as to
the quality and performance of the software is with the user.

---

## 8. Limitation of Liability

In no event shall the author, contributors, or copyright holders be liable for
any claim, damage, or other liability, whether in an action of contract, tort,
or otherwise, arising from, out of, or in connection with the software or the
use or other dealings in the software.

This limitation applies to:

- Civil liability arising from unauthorized use of the software by a third party
  who obtained the software from you.
- Criminal liability for the acts of any person who uses the software without
  your authorization.
- Damages arising from interference with third-party communications.
- Loss of data, revenue, or business arising from use of the software.

---

## 9. Indemnification

By using this software you agree to indemnify, defend, and hold harmless the
author and contributors from any claim, demand, liability, loss, cost, or
expense (including legal fees) arising out of or relating to your use of the
software, your violation of this legal notice, or your violation of any law
or the rights of a third party.

---

## 10. Export Control

This software may be subject to export control laws and regulations, including
but not limited to the U.S. Export Administration Regulations (EAR) and the
International Traffic in Arms Regulations (ITAR). You are responsible for
compliance with all applicable export control laws and regulations in your
jurisdiction before distributing or deploying this software.

---

## 11. Modifications and Derivative Works

You may modify and distribute this software under the terms of the MIT License.
However:

- You must retain this legal notice in any derivative work.
- You may not remove or modify the rate limiting, authentication, or input
  validation controls in a derivative work for the purpose of enabling use
  against systems you do not have authorization to test.
- Derivative works that add capabilities not present in this firmware (e.g.,
  automated scanning of public IP addresses, mass-deployment tooling, C2
  integration) are subject to additional legal restrictions and you are solely
  responsible for compliance.

---

## 12. Contact and Reporting Misuse

To report misuse of this software, contact the repository maintainer via the
GitHub repository at <https://github.com/MehaanM/Operation-Phantom32_Protocol>.

If you are law enforcement investigating a misuse incident, please open a
GitHub Security Advisory or contact the maintainer directly.

---

## 13. Acknowledgment

By using, building, flashing, or deploying Phantom32 firmware, you acknowledge
that:

- You have read and understood this legal notice.
- You are using the software only in environments you own or for which you have
  explicit written authorization.
- You understand that unauthorized use of the capabilities in this firmware is
  a criminal offense in your jurisdiction.
- You accept all legal and financial responsibility for your use of this
  software.

---

*This legal notice is not a substitute for professional legal advice. If you
are uncertain whether a particular use is authorized, do not proceed until you
have consulted a licensed attorney in your jurisdiction.*
