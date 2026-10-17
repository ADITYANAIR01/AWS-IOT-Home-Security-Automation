# AWS Cloud Setup — Outputs

Region: `ap-south-1` (Mumbai) · Account: `121490076448`

## AWS IoT Core
| Item | Value |
|---|---|
| Thing | `home-security-esp32` |
| IoT Data Endpoint | `adndeuo6w4mnl-ats.iot.ap-south-1.amazonaws.com` |
| Cert ARN | `arn:aws:iot:ap-south-1:121490076448:cert/31bca7bb21cd63d3263eec7bc560b1e452c08de76337cd4ec9bb690164cba791` |
| Device policy | `home-security-esp32-publish` (connect as `home-security-esp32`, publish to `home/security/sensors`) |
| MQTT topic | `home/security/sensors` |

## Certificates (gitignored, in `certs/`)
- `certs/device.pem.crt` — device certificate
- `certs/private.pem.key` — private key (KEEP SECRET)
- `certs/public.pem.key` — public key
- `certs/AmazonRootCA1.pem` — Amazon root CA

## Storage
| Item | Value |
|---|---|
| DynamoDB table | `SensorReadings` (PK `deviceId` S, SK `timestamp` N, on-demand billing) |

## Alerts
| Item | Value |
|---|---|
| SNS topic | `arn:aws:sns:ap-south-1:121490076448:home-security-alerts` |
| Email subscription | adityanairtech@gmail.com — ⚠️ **PENDING CONFIRMATION — check inbox/spam and click the confirm link** |

## IoT Rules
| Rule | Trigger | Action |
|---|---|---|
| `home_security_persist` | all messages on `home/security/sensors` | PutItem → `SensorReadings` (adds `deviceId`, `ts` epoch-ms) |
| `home_security_alert` | `fire = 'Fire_Detected'` OR `lpg = 'LPG_Leak_Detected'` | SNS publish → email alert |
| IAM role | `home-security-iot-rule-role` | dynamodb:PutItem + sns:Publish |

## Dashboard auth (guest/open access)
| Item | Value |
|---|---|
| Cognito Identity Pool | `ap-south-1:daac6d6a-4c03-441a-bb3c-a16f24f478f3` (guest access only, no user pool) |
| Guest role | `home-security-guest-role` — iot:Connect, iot:Subscribe (topicfilter), iot:Receive on sensor topic only (no publish) |

## Dashboard hosting (S3 + CloudFront)
| Item | Value |
|---|---|
| S3 bucket | `iot-adityanair-tech-dashboard` (ap-south-1, fully private, OAC-only) |
| CloudFront distribution | `EHTL2WMRZT8XW` → `d2quyry4cgda3q.cloudfront.net` |
| Custom domain | `iot.adityanair.tech` (alias added after cert validates) |
| ACM cert (us-east-1) | `arn:aws:acm:us-east-1:121490076448:certificate/ae57cb76-2672-499e-a15a-e72b6c4dede1` — PENDING DNS validation |
| DNS validation CNAME | `_ff9256473c4b802d6aaf38e0cffb822a.iot.adityanair.tech` → `_537ea43d1a4d19fa65a4401a4b7466a7.wzccmgtwzk.acm-validations.aws.` |
| DNS alias CNAME (add after cert validates) | `iot.adityanair.tech` → `d2quyry4cgda3q.cloudfront.net` |

## ⚠️ Temporary open access
The dashboard is open to anyone with the URL (guest identity pool). To close it later:
`aws cognito-identity update-identity-pool --identity-pool-id ap-south-1:daac6d6a-4c03-441a-bb3c-a16f24f478f3 --identity-pool-name home-security-guest --allow-unauthenticated-identities --no-allow-classic-flow` (keep as-is) — or simply detach the guest role / set `allow-unauthenticated-identities` to false.
