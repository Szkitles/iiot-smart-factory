# VPS Deployment Guide (CI/CD)

This guide explains how to set up a remote virtual private server (VPS) running Linux (Ubuntu) to host this IIoT stack and link it with the GitHub Actions deployment pipeline.

---

## Step 1: Prepare the VPS
1. Get a Linux VPS (Ubuntu 22.04 LTS is recommended) from a cloud provider (e.g., DigitalOcean, AWS, OVH, Linode).
2. SSH into your VPS:
   ```bash
   ssh root@your_vps_ip
   ```
3. Install Docker and Docker Compose on the VPS:
   ```bash
   sudo apt-get update
   sudo apt-get install -y docker.io docker-compose
   sudo systemctl enable --now docker
   ```
4. Clone your repository into the home directory of the VPS:
   ```bash
   cd ~
   git clone https://github.com/your-username/iiot-smart-factory.git
   ```

---

## Step 2: Set up SSH Keys for GitHub Actions
To allow GitHub Actions to safely connect to your VPS without a password, we use SSH keys.

1. Generate a new SSH key pair **on your local machine or the VPS**:
   ```bash
   ssh-keygen -t rsa -b 4096 -C "github-actions-deploy" -f ~/.ssh/vps_deploy_key
   ```
   *(Press Enter to skip entering a passphrase).*

2. Add the **Public Key** (`~/.ssh/vps_deploy_key.pub`) to the authorized keys on your VPS:
   ```bash
   cat ~/.ssh/vps_deploy_key.pub >> ~/.ssh/authorized_keys
   chmod 600 ~/.ssh/authorized_keys
   chmod 700 ~/.ssh
   ```

3. Copy the entire content of the **Private Key** (`~/.ssh/vps_deploy_key`). You will need this for GitHub.

---

## Step 3: Configure GitHub Secrets
Go to your repository on GitHub:
1. Navigate to **Settings -> Secrets and variables -> Actions**.
2. Click **New repository secret** and add the following three secrets:
   - **`VPS_IP`**: The public IP address of your VPS (e.g., `123.45.67.89`).
   - **`VPS_USER`**: The SSH username (typically `root` or `ubuntu`).
   - **`VPS_SSH_KEY`**: Paste the entire content of the **Private Key** (including `-----BEGIN RSA PRIVATE KEY-----` and `-----END RSA PRIVATE KEY-----`).

---

## Step 4: Run the First Deploy
1. The first time you deploy, you must manually run the containers on the VPS to build the named volumes and environment:
   ```bash
   cd ~/iiot-smart-factory
   docker-compose up -d --build
   ```
2. From now on, every time you run `git push origin master`, GitHub Actions will trigger, connect to the VPS, pull the latest code, and restart the containers automatically.
